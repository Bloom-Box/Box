#pragma once
#include <stdint.h>
#include <stddef.h>

enum class Type : uint8_t { OFFER=1, ACK, DATA, READY, REJECT, FINISH, CANCEL };

struct Packet {
  virtual ~Packet() {}
  virtual Type type() const = 0;
  virtual size_t serialize(uint8_t* out) const = 0;
  static Packet* parse(const uint8_t* in, size_t len);
};

// ---------- Offer ----------
struct OfferPacket : Packet {
  uint32_t size;
  uint8_t  sha256[32];
  uint32_t version;
  Type type() const override;
  size_t serialize(uint8_t* out) const override;
  static OfferPacket* from(const uint8_t* in, size_t len);
};

// ---------- Ack ----------
struct AckPacket : Packet {
  uint32_t nextExpected;
  Type type() const override;
  size_t serialize(uint8_t* out) const override;
  static AckPacket* from(const uint8_t* in, size_t len);
};

// ---------- Data ----------
struct DataPacket : Packet {
  uint32_t seq;
  uint16_t len;
  uint16_t crc16;
  uint8_t  bytes[200];
  Type type() const override;
  size_t serialize(uint8_t* out) const override;
  static DataPacket* from(const uint8_t* in, size_t len);
};

// ---------- Control ----------
struct ControlPacket : Packet {
  Type tag; // one of READY, REJECT, FIN, CANCEL
  explicit ControlPacket(Type t) : tag(t) {}
  Type type() const override;
  size_t serialize(uint8_t* out) const override;
  static ControlPacket* from(const uint8_t* in, size_t len);
};
