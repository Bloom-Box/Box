#include "../include/ntcp/main.h"
#include <string.h>

Ntcp* Ntcp::instance = nullptr;

Ntcp::Ntcp() {}

bool Ntcp::begin(LogFn log){
  logFn = log;
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return false;
  esp_now_register_send_cb(&Ntcp::onSendCallback);
  esp_now_register_recv_cb(&Ntcp::onRecvCallback);
  instance = this;
  return true;
}

void Ntcp::listen() {}

bool Ntcp::connect(const uint8_t peer[6], uint32_t synSeq, uint32_t timeoutMs){
  Peer* p = findOrCreatePeer(peer);
  p->state = ST_SYN_SENT; p->txSeq = synSeq; p->rxNext = 0; p->lastSeen = millis();
  return sendControl(NTCP_SYN, nullptr, 0, peer, timeoutMs);
}

bool Ntcp::send(const uint8_t* data, uint16_t len, const uint8_t* peerMac){
  Peer* p = findOrCreatePeer(peerMac);
  if (!p || p->state != ST_ESTABLISHED) return false;
  NtcpSegment seg{}; seg.hdr.seq = p->txSeq; seg.hdr.ack = p->rxNext; seg.hdr.flags = NTCP_ACK; seg.hdr.len = len;
  memcpy(seg.data, data, len); seg.hdr.csum = calcChecksum(seg);
  if (!sendWithRetries(seg, peerMac, 500*5)) return false;
  p->txSeq += len;
  p->lastSeen = millis();
  return true;
}

Peer* Ntcp::findOrCreatePeer(const uint8_t* mac){
  for(auto& p : peers) if(memcmp(p.mac, mac, 6)==0) return &p;
  peers.push_back({});
  Peer& p = peers.back(); memcpy(p.mac, mac, 6); p.txSeq = 1; p.rxNext = 0; p.state = ST_ESTABLISHED; p.lastSeen = millis();
  esp_now_peer_info_t peerInfo{}; memcpy(peerInfo.peer_addr, mac, 6); peerInfo.channel=0; peerInfo.encrypt=false;
  esp_now_add_peer(&peerInfo);
  return &p;
}

uint16_t Ntcp::calcChecksum(const NtcpSegment& seg){
  uint32_t sum=0; NtcpHeader htmp=seg.hdr; htmp.csum=0;
  const uint8_t* ph=reinterpret_cast<const uint8_t*>(&htmp);
  for(size_t i=0;i<sizeof(NtcpHeader);++i) sum+=ph[i];
  for(uint16_t i=0;i<seg.hdr.len;++i) sum+=seg.data[i];
  while(sum>>16) sum=(sum&0xFFFF)+(sum>>16);
  return static_cast<uint16_t>(~sum);
}

bool Ntcp::sendRaw(const NtcpSegment& seg, const uint8_t* peerMac){ return esp_now_send(peerMac,reinterpret_cast<const uint8_t*>(&seg),sizeof(NtcpHeader)+seg.hdr.len)==ESP_OK; }

bool Ntcp::sendWithRetries(NtcpSegment& seg,const uint8_t* peerMac,uint32_t timeoutMs){
  uint32_t start=millis(); uint8_t tries=0;
  while(millis()-start<timeoutMs && tries<=5){ sendRaw(seg,peerMac); uint32_t t0=millis(); while(millis()-t0<500){ Peer* p=findOrCreatePeer(peerMac); if(p->rxNext >= seg.hdr.seq+seg.hdr.len){ p->lastSeen=millis(); return true; } delay(1); } ++tries; }
  log("sendWithRetries timeout"); return false;
}

bool Ntcp::sendControl(uint8_t flags,const uint8_t* data,uint16_t len,const uint8_t* peerMac,uint32_t timeoutMs){
  NtcpSegment seg{}; seg.hdr.seq=findOrCreatePeer(peerMac)->txSeq; seg.hdr.ack=findOrCreatePeer(peerMac)->rxNext;
  seg.hdr.flags=flags; seg.hdr.len=len; if(data&&len) memcpy(seg.data,data,len); seg.hdr.csum=calcChecksum(seg);
  return sendWithRetries(seg,peerMac,timeoutMs);
}

void Ntcp::heartbeat(){
  uint32_t now=millis();
  for(auto& p : peers){
    if(now - p.lastSeen > 1000){ // sende Heartbeat
      NtcpSegment seg{}; seg.hdr.seq=p.txSeq; seg.hdr.ack=p.rxNext; seg.hdr.flags=NTCP_META; seg.hdr.len=0; seg.hdr.csum=calcChecksum(seg);
      sendRaw(seg,p.mac);
      p.lastSeen=now;
    }
  }
}

void Ntcp::onSendCallback(const uint8_t* macAddr, esp_now_send_status_t status){ (void)macAddr; (void)status; }
void Ntcp::onRecvCallback(const uint8_t* macAddr,const uint8_t* data,int len){ if(instance) instance->handleRecv(macAddr,data,len); }

void Ntcp::handleRecv(const uint8_t* macAddr,const uint8_t* data,int len){
  if(len<(int)sizeof(NtcpHeader)) return;
  NtcpSegment seg{}; memcpy(&seg,data,len);
  if(calcChecksum(seg)!=seg.hdr.csum){ log("checksum fail"); return; }
  Peer* p=findOrCreatePeer(macAddr);
  if(seg.hdr.ack>p->txSeq) p->txSeq=seg.hdr.ack;
  p->lastSeen=millis();
  if(p->state==ST_SYN_SENT && (seg.hdr.flags & NTCP_SYN)){ p->rxNext=seg.hdr.seq+1; sendControl(NTCP_ACK,nullptr,0,macAddr,500*5); p->state=ST_ESTABLISHED; }
  if(seg.hdr.len>0 && rxHandler) rxHandler(seg.data,seg.hdr.len,macAddr,seg.hdr.flags);
}