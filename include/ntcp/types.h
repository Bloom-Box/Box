#pragma once
#include <functional>
#include <vector>
#include <Arduino.h>

#pragma pack(push,1)
struct NtcpHeader {
  uint32_t seq;
  uint32_t ack;
  uint8_t flags;
  uint16_t len;
  uint16_t csum;
};
#pragma pack(pop)

struct NtcpSegment {
  NtcpHeader hdr;
  uint8_t data[200];
};

enum NtcpFlags : uint8_t { NTCP_SYN=0x01, NTCP_ACK=0x02, NTCP_FIN=0x04, NTCP_RST=0x08, NTCP_META=0x10 };
using RxHandler = std::function<void(const uint8_t* data, uint16_t len, const uint8_t* mac, uint8_t flags)>;
using LogFn = std::function<void(const String&)>;