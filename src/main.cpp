// Receiver.ino
#include <WiFi.h>
#include <esp_now.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include "Packet.h"

static uint32_t expectedSeq=0;
static mbedtls_sha256_context sha;
static uint8_t offerHash[32];
static bool hashing=false;

static uint16_t crc16(const uint8_t* d,size_t n){
  uint16_t c=0xFFFF; for(size_t i=0;i<n;i++){ c^=(uint16_t)d[i]<<8; for(int j=0;j<8;j++) c=(c&0x8000)?(c<<1)^0x1021:(c<<1); }
  return c;
}
static void sendAck(const uint8_t* mac,uint32_t next){
  AckPacket a; a.nextExpected = next; uint8_t buf[8]; 
  esp_now_send(mac, buf, a.serialize(buf));
}
static void sendControl(const uint8_t* mac, Type t) {
  ControlPacket c (t); uint8_t buf[1];
  esp_now_send(mac, buf, c.serialize(buf));
}


void onRecv(const uint8_t* mac,const uint8_t* data,int len) {

  if (len<1) return;

  Packet* pkt = Packet::parse(data,len);
  if (!pkt) return;

  switch (pkt->type()) {

    case Type::OFFER: {

      auto* o = static_cast<OfferPacket*>(pkt);

      expectedSeq=0;

      memcpy(offerHash, o->sha256, 32);

      if (Update.begin(o->size)) {

        mbedtls_sha256_init(&sha); 
        mbedtls_sha256_starts_ret(&sha, 0); 
        hashing=true;

        sendControl(mac, Type::FINISH);

      } else sendControl(mac, Type::REJECT);

      break;

    } 

    case Type::DATA: {

      auto* d = static_cast<DataPacket*>(pkt);

      if (d->seq != expectedSeq || crc16(d->bytes, d->len) != d->crc16) { sendAck(mac, expectedSeq); break; }

      size_t w = Update.write(d->bytes,d->len);
      if (w != d->len) { sendControl(mac, Type::CANCEL); break; }

      if (hashing) mbedtls_sha256_update_ret(&sha, d->bytes, d->len);

      expectedSeq++;

      sendAck(mac, expectedSeq);

      break;

    } 

    case Type::FINISH: {

        if (hashing) { 

            uint8_t calc[32]; 
            mbedtls_sha256_finish_ret(&sha, calc); 

            hashing = false; 

            if (memcmp(calc, offerHash, 32) != 0) return sendControl(mac, Type::CANCEL);

        }

        if (!Update.end(true)) return sendControl(mac, Type::CANCEL);

        sendControl(mac, Type::FINISH);
        delay(100); 
        ESP.restart();

        break;
    }

    case Type::READY:
    case Type::REJECT:
    case Type::CANCEL:
    case Type::ACK:
    default: break;

  }

  delete pkt;

}

void setup(){

  WiFi.mode(WIFI_STA); WiFi.disconnect();
  if(esp_now_init() != ESP_OK) ESP.restart();

  esp_now_register_recv_cb(onRecv);

  uint8_t senderMac[6]  = {0xE4, 0x65, 0xB8, 0x7E, 0x22, 0x50};
  esp_now_peer_info_t p={0}; memcpy(p.peer_addr, senderMac, 6); p.channel=0; p.encrypt=false; esp_now_add_peer(&p);

}

void loop() {}
