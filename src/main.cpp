#include <WiFi.h>
#include <esp_now.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include "Packet.h"
#include <Arduino.h>

// Debug print function with timestamp and color
void debugPrint(const char* format, ...){
    va_list args;
    va_start(args, format);
    Serial.print("[");
    Serial.print(millis());
    Serial.print(" ms] ");
    vprintf(format, args);
    Serial.println();
    va_end(args);
}

// Global variables
static uint32_t expectedSeq=0;
static mbedtls_sha256_context sha;
static uint8_t offerHash[32];
static bool hashing=false;

static uint16_t crc16(const uint8_t* d,size_t n){
  uint16_t c=0xFFFF; 
  for(size_t i=0;i<n;i++){
    c^=(uint16_t)d[i]<<8; 
    for(int j=0;j<8;j++) c=(c&0x8000)?(c<<1)^0x1021:(c<<1); 
  }
  return c;
}

static void sendAck(const uint8_t* mac,uint32_t next){
  AckPacket a; 
  a.nextExpected = next; 
  uint8_t buf[8]; 
  size_t len = a.serialize(buf);
  debugPrint("Sending ACK for sequence=%u", next);
  esp_now_send(mac, buf, len);
}

static void sendControl(const uint8_t* mac, Type t) {
  ControlPacket c(t); 
  uint8_t buf[1];
  size_t len = c.serialize(buf);
  const char* typeStr = "";
  switch(t){
    case Type::READY: typeStr="READY"; break;
    case Type::REJECT: typeStr="REJECT"; break;
    case Type::FINISH: typeStr="FINISH"; break;
    case Type::CANCEL: typeStr="CANCEL"; break;
    default: typeStr="UNKNOWN"; break;
  }
  debugPrint("Sending Control packet: %s", typeStr);
  esp_now_send(mac, buf, len);
}

void onRecv(const uint8_t* mac,const uint8_t* data,int len) {
    debugPrint("Packet received from %02X:%02X:%02X:%02X:%02X:%02X, length=%d", 
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], len);

    if (len<1) {
        debugPrint("Received packet too short, ignoring");
        return;
    }

    Packet* pkt = Packet::parse(data,len);
    if (!pkt) {
        debugPrint("Failed to parse packet");
        return;
    }

    debugPrint("Parsed packet of type %d", static_cast<int>(pkt->type()));

    switch (pkt->type()) {
        case Type::OFFER: {
            auto* o = static_cast<OfferPacket*>(pkt);
            debugPrint("Received OFFER packet: size=%u, version=%u", o->size, o->version);

            expectedSeq=0;
            memcpy(offerHash, o->sha256, 32);
            debugPrint("Stored expected SHA256 hash");

            if (Update.begin(o->size)) {
                debugPrint("Update started, size=%u", o->size);
                mbedtls_sha256_init(&sha); 
                mbedtls_sha256_starts_ret(&sha, 0); 
                hashing=true;
                debugPrint("SHA-256 hashing initiated");
                sendControl(mac, Type::FINISH);
                debugPrint("Sent FINISH control packet");
            } else {
                debugPrint("Failed to begin update");
                sendControl(mac, Type::REJECT);
            }
            break;
        }
        case Type::DATA: {
            auto* d = static_cast<DataPacket*>(pkt);
            debugPrint("Received DATA packet: seq=%u, len=%u, crc=%04X", d->seq, d->len, d->crc16);

            if (d->seq != expectedSeq || crc16(d->bytes, d->len) != d->crc16) {
                debugPrint("Sequence mismatch or CRC error, expected=%u", expectedSeq);
                sendAck(mac, expectedSeq);
                debugPrint("Sent ACK for expected=%u", expectedSeq);
                break;
            }

            size_t w = Update.write(d->bytes,d->len);
            if (w != d->len) {
                debugPrint("Write to flash failed, wrote=%zu", w);
                sendControl(mac, Type::CANCEL);
                break;
            }
            debugPrint("Wrote %u bytes to flash", w);

            if (hashing) {
                mbedtls_sha256_update_ret(&sha, d->bytes, d->len);
                debugPrint("Updated SHA-256 hash");
            }

            expectedSeq++;
            sendAck(mac, expectedSeq);
            debugPrint("Sent ACK for next expected=%u", expectedSeq);
            break;
        }
        case Type::FINISH: {
            debugPrint("Received FINISH");
            if (hashing) {
                uint8_t calc[32];
                mbedtls_sha256_finish_ret(&sha, calc);
                debugPrint("SHA-256 hash computed");
                hashing = false;

                if (memcmp(calc, offerHash, 32) != 0) {
                    debugPrint("SHA-256 mismatch! Update invalid");
                    return sendControl(mac, Type::CANCEL);
                } else {
                    debugPrint("SHA-256 verified");
                }
            }

            if (!Update.end(true)) {
                debugPrint("Update end failed");
                return sendControl(mac, Type::CANCEL);
            }

            debugPrint("Update completed successfully");
            sendControl(mac, Type::FINISH);
            delay(100);
            debugPrint("Restarting...");
            ESP.restart();

            break;
        }
        default:
            debugPrint("Received unhandled packet type");
            break;
    }

    delete pkt;
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    debugPrint("Initializing WiFi");
    WiFi.mode(WIFI_STA); 
    WiFi.disconnect();

    if(esp_now_init() != ESP_OK){
        debugPrint("ESP-NOW initialization failed");
        ESP.restart();
    }
    debugPrint("ESP-NOW initialized");
    esp_now_register_recv_cb(onRecv);

    uint8_t senderMac[6]  = {0xE4, 0x65, 0xB8, 0x7E, 0x22, 0x50};
    esp_now_peer_info_t p={0}; 
    memcpy(p.peer_addr, senderMac, 6); 
    p.channel=0; p.encrypt=false; 
    esp_now_add_peer(&p);
    debugPrint("Added peer %02X:%02X:%02X:%02X:%02X:%02X", senderMac[0], senderMac[1], senderMac[2], senderMac[3], senderMac[4], senderMac[5]);
}

void loop() {
    // Nothing to do here
}