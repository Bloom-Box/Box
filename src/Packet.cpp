#include "Packet.h"
#include <string.h>

// helpers
static inline void wr32(uint8_t* p, uint32_t v){ memcpy(p,&v,4); }
static inline void wr16(uint8_t* p, uint16_t v){ memcpy(p,&v,2); }

// ---- Offer ----
Type OfferPacket::type() const { return Type::OFFER; }
size_t OfferPacket::serialize(uint8_t* out) const {
  out[0]=(uint8_t)Type::OFFER;
  wr32(out+1,size);
  memcpy(out+5,sha256,32);
  wr32(out+37,version);
  return 41;
}
OfferPacket* OfferPacket::from(const uint8_t* in, size_t L){
  if(L!=41 || in[0]!=(uint8_t)Type::OFFER) return nullptr;
  auto* p=new OfferPacket();
  memcpy(&p->size,in+1,4);
  memcpy(p->sha256,in+5,32);
  memcpy(&p->version,in+37,4);
  return p;
}

// ---- Ack ----
Type AckPacket::type() const { return Type::ACK; }
size_t AckPacket::serialize(uint8_t* out) const {
  out[0]=(uint8_t)Type::ACK; wr32(out+1,nextExpected); return 5;
}
AckPacket* AckPacket::from(const uint8_t* in, size_t L){
  if(L!=5 || in[0]!=(uint8_t)Type::ACK) return nullptr;
  auto* p=new AckPacket(); memcpy(&p->nextExpected,in+1,4); return p;
}

// ---- Data ----
Type DataPacket::type() const { return Type::DATA; }
size_t DataPacket::serialize(uint8_t* out) const {
  out[0]=(uint8_t)Type::DATA;
  wr32(out+1,seq); wr16(out+5,len); wr16(out+7,crc16);
  memcpy(out+9,bytes,len);
  return 9+len;
}
DataPacket* DataPacket::from(const uint8_t* in, size_t L){
  if(L<9 || in[0]!=(uint8_t)Type::DATA) return nullptr;
  uint16_t plen; memcpy(&plen,in+5,2);
  if(plen>200 || L!=9+plen) return nullptr;
  auto* p=new DataPacket();
  memcpy(&p->seq,in+1,4);
  p->len=plen;
  memcpy(&p->crc16,in+7,2);
  memcpy(p->bytes,in+9,plen);
  return p;
}

// ---- Control ----
Type ControlPacket::type() const { return tag; }
size_t ControlPacket::serialize(uint8_t* out) const {
  out[0]=(uint8_t)tag; return 1;
}
ControlPacket* ControlPacket::from(const uint8_t* in, size_t L){
  if(L!=1) return nullptr;
  Type t=(Type)in[0];
  switch(t){
    case Type::READY: case Type::REJECT: case Type::FINISH: case Type::CANCEL:
      return new ControlPacket(t);
    default: return nullptr;
  }
}

// ---- Factory ----
Packet* Packet::parse(const uint8_t* in, size_t L){
  if(L<1) return nullptr;
  switch((Type)in[0]){
    case Type::OFFER: return OfferPacket::from(in,L);
    case Type::ACK:   return AckPacket::from(in,L);
    case Type::DATA:  return DataPacket::from(in,L);
    case Type::READY:
    case Type::REJECT:
    case Type::FINISH:
    case Type::CANCEL: return ControlPacket::from(in,L);
    default: return nullptr;
  }
}
