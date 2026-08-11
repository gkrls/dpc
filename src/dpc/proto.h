#ifndef DPC_PROTO_H
#define DPC_PROTO_H

#include "dpc/device.h"
#include "dpc/task.h"
#include <cstdint>
#include <netinet/in.h>

namespace dpc {

using bitmap_t = uint32_t;
using flags_t = uint8_t;
using quant_t = uint32_t;
using value_t = uint32_t;

enum Flag : uint8_t {
  F_BA = 0b10000000,
  F_OL = 0b01000000,
  F_U1 = 0b00100000,
  F_U2 = 0b00010000,
  F_SY = 0b00001000,
  F_CK = 0b00000100,
  F_RE = F_U1,
  F_VER = F_U2,

  OF_PIPE = 0b00000011,
  OF_COLL = 0b00001100,
  OF_OPER = 0b01110000,
};

struct Flags {
  flags_t flags;

};


inline static flags_t f_check(flags_t flags, flags_t flag) { return flags & flag; }
inline static flags_t f_set(flags_t flags, flags_t flag) { return flags | flag; }
inline static flags_t f_clear(flags_t flags, flags_t flag) { return flags & ~flag; }
inline static flags_t f_toggle(flags_t flags, flags_t flag) { return flags ^ flag; }

inline static uint8_t f_pipes(flags_t flags) { return (flags & OF_PIPE); }
inline static uint8_t f_getpipes(flags_t flags) { return f_pipes(flags) + 1; }
inline static uint8_t f_setpipes(uint8_t num_pipes) { return num_pipes - 1; }

inline static bool f_missmatch(flags_t a, flags_t b, flags_t flag) { return f_check(a, flag) != f_check(b, flag); }

inline static Collective op_coll(uint8_t op) { return static_cast<Collective>((op & Flag::OF_COLL) >> 2); }
inline static uint8_t op_coll(uint8_t op, Collective coll) {
  return (op & ~Flag::OF_COLL) | ((static_cast<uint8_t>(coll) << 2) & Flag::OF_COLL);
}

struct Header {
  uint32_t sessid;    // 4 bytes
  uint32_t operid;    // 4 bytes - unused by device
  uint32_t offset;    // 4 bytes - unused by device
  uint16_t slotid;    // 2 bytes
  uint8_t rank;       // 1 byte
  uint8_t world;      // 1 byte
  uint8_t oflags;     // 1 byte
  uint8_t flags;      // 1 byte
  uint16_t counts;    // 2 bytes(4+12) - unused by device
  uint32_t quants;    // 4 bytes

  inline void qvcount(uint16_t qvcount) { counts = qvcount; }
  inline void qvcount(uint8_t q, uint16_t v) { counts = Header::getqvcount(q, v); }
  inline uint8_t qcount() const { return Header::getqcount(counts); }
  inline int16_t vcount() const { return Header::getvcount(counts); }

  inline uint8_t pipes() const { return f_getpipes(flags); }
  inline bool bad() const { return f_check(flags, F_BA); }
  inline bool old() const { return f_check(flags, F_OL); }
  inline bool syn() const { return f_check(flags, F_SY); }
  inline bool cntk() const { return f_check(flags, F_CK); }
  inline bool re() const { return f_check(flags, F_RE); }

  static uint8_t getqcount(uint16_t qvcount) { return (qvcount >> 12); }
  static uint16_t getvcount(uint16_t qvcount) { return qvcount & 0xfff; }
  static uint16_t getqvcount(uint8_t q, uint16_t v) { return (v & 0xFFF) | ((uint16_t)(q & 0x0F) << 12); }
} __attribute__((packed)); // 32 bytes;

struct Packet : public Header {
public:
  using header_t = Header;
  Header *header() { return this; }
  inline uint32_t *payload() {
    return reinterpret_cast<uint32_t *>(reinterpret_cast<char *>(this) + sizeof(Header));
  }
  inline uint32_t const *payload() const {
    return reinterpret_cast<uint32_t const *>(reinterpret_cast<char const *>(this) + sizeof(Header));
  }
  inline uint32_t payloadLen(Device const &dev) const { return pipes() * dev.conf.valuesPerPipe(); }
  inline uint32_t payloadSize(Device const &dev) const { return pipes() * dev.conf.valuesPerPipe() * 4; }
  inline uint32_t size(Device const &dev) const { return sizeof(Header) + payloadSize(dev); }
  // If we ever support values other than 4-bytes, add
  // inline uint16_t *payload() { ... } etc..
  inline void htonHeader() {
    sessid = htonl(sessid);
    offset = htonl(offset);
    counts = htons(counts);
    quants = htonl(quants);
  }
  inline void ntohHeader() {
    sessid = ntohl(sessid);
    offset = ntohl(offset);
    counts = ntohs(counts);
    quants = ntohl(quants);
  }

public:
  static inline uint32_t maxPayloadLen(Device const &dev) { return dev.conf.pipes * dev.conf.reducers * dev.conf.reducer_mode; }
  static inline uint32_t maxPayloadSize(Device const &dev) { return maxPayloadLen(dev) * 4; }
  static inline uint32_t maxSize(Device const &dev) { return sizeof(Header) + maxPayloadSize(dev); }
} __attribute__((packed));

} // namespace dpc

#endif // !DPC_PROTO_H
