#pragma once
#include "types.h"
#include <WiFi.h>
#include <esp_now.h>
#include <vector>

class Ntcp {
public:
  Ntcp();
  bool begin(LogFn log = nullptr);
  bool connect(const uint8_t peer[6], uint32_t synSeq = 1, uint32_t timeoutMs = 2000);
  void listen();
  bool send(const uint8_t* data, uint16_t len, const uint8_t* peerMac);
  void onReceive(RxHandler cb);
  void heartbeat();
  static uint16_t calcChecksum(const NtcpSegment& seg);

private:
  enum State { ST_CLOSED, ST_LISTEN, ST_SYN_SENT, ST_ESTABLISHED };
  static Ntcp* instance;
  struct Peer { uint8_t mac[6]; uint32_t txSeq; uint32_t rxNext; State state; uint32_t lastSeen; };
  std::vector<Peer> peers;
  RxHandler rxHandler = nullptr;
  LogFn logFn = nullptr;

  Peer* findOrCreatePeer(const uint8_t* mac);
  bool sendRaw(const NtcpSegment& seg, const uint8_t* peerMac);
  bool sendWithRetries(NtcpSegment& seg, const uint8_t* peerMac, uint32_t timeoutMs);
  bool sendControl(uint8_t flags, const uint8_t* data, uint16_t len, const uint8_t* peerMac, uint32_t timeoutMs);
  static void onSendCallback(const uint8_t* macAddr, esp_now_send_status_t status);
  static void onRecvCallback(const uint8_t* macAddr, const uint8_t* data, int len);
  void handleRecv(const uint8_t* macAddr, const uint8_t* data, int len);
  void log(const String& s){ if(logFn) logFn(s); }
};