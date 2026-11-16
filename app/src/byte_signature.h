// Finds a stretch of the title's data by its hashes (bo2-dev).
//
// The patches never carry the title's own bytes: a stretch to patch is named by its length, a
// 16-bit tag of its first 4 bytes (a quick filter) and the 64-bit FNV-1a hash of all of it. The
// edits then write only bo2-dev's own values (numbers, jumps, text keys) at offsets inside it.
// A different build of the data has different hashes and is left alone.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace bo2 {

struct ByteSignature {
  uint32_t length;
  uint16_t tag;   // (big-endian u32 of the first 4 bytes * 0x9E3779B1) >> 16
  uint64_t fnv;   // FNV-1a 64 of the whole stretch
};

struct ByteEdit {
  uint32_t offset;
  std::string bytes;
};

inline uint64_t Fnv1a64(const uint8_t* data, size_t size) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (size_t i = 0; i < size; ++i) {
    h = (h ^ data[i]) * 0x100000001b3ull;
  }
  return h;
}

inline uint16_t Tag(const uint8_t* p) {
  const uint32_t v = (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
  return static_cast<uint16_t>((v * 0x9E3779B1u) >> 16);
}

inline bool Matches(const ByteSignature& sig, const uint8_t* p) {
  return Tag(p) == sig.tag && Fnv1a64(p, sig.length) == sig.fnv;
}

// Offset of the first match in data[from, size), or SIZE_MAX.
inline size_t Find(const ByteSignature& sig, const uint8_t* data, size_t size, size_t from = 0) {
  if (sig.length < 4 || size < sig.length) {
    return SIZE_MAX;
  }
  for (size_t i = from; i + sig.length <= size; ++i) {
    if (Matches(sig, data + i)) {
      return i;
    }
  }
  return SIZE_MAX;
}

inline void ApplyEdits(uint8_t* at, const std::vector<ByteEdit>& edits) {
  for (const ByteEdit& edit : edits) {
    for (size_t k = 0; k < edit.bytes.size(); ++k) {
      at[edit.offset + k] = static_cast<uint8_t>(edit.bytes[k]);
    }
  }
}

// Big-endian encodings of bo2-dev's own values.
inline std::string BE32(uint32_t v) {
  return std::string{static_cast<char>(v >> 24), static_cast<char>(v >> 16),
                     static_cast<char>(v >> 8), static_cast<char>(v)};
}

inline std::string FloatBE(float value) {
  uint32_t bits;
  static_assert(sizeof(bits) == sizeof(value));
  std::memcpy(&bits, &value, sizeof(bits));
  return BE32(bits);
}

}  // namespace bo2
