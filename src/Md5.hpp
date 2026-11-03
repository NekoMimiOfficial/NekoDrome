#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

namespace nt {

inline std::string md5Hex(const std::string& input) {
  static uint32_t K[64];
  static bool init = false;
  if (!init) {
    for (int i = 0; i < 64; ++i) K[i] = (uint32_t)std::floor(std::fabs(std::sin((double)i + 1.0)) * 4294967296.0);
    init = true;
  }
  static const int S[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9,  14, 20, 5, 9,
                            14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
                            4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
  uint32_t a0 = 0x67452301, b0 = 0xefcdab89, c0 = 0x98badcfe, d0 = 0x10325476;
  std::string msg = input;
  uint64_t bits = (uint64_t)input.size() * 8;
  msg += (char)0x80;
  while (msg.size() % 64 != 56) msg += (char)0;
  for (int i = 0; i < 8; ++i) msg += (char)((bits >> (8 * i)) & 0xff);
  for (size_t off = 0; off < msg.size(); off += 64) {
    uint32_t M[16];
    for (int i = 0; i < 16; ++i) {
      const unsigned char* p = (const unsigned char*)msg.data() + off + i * 4;
      M[i] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    uint32_t A = a0, B = b0, C = c0, D = d0;
    for (int i = 0; i < 64; ++i) {
      uint32_t F;
      int g;
      if (i < 16) { F = (B & C) | (~B & D); g = i; }
      else if (i < 32) { F = (D & B) | (~D & C); g = (5 * i + 1) % 16; }
      else if (i < 48) { F = B ^ C ^ D; g = (3 * i + 5) % 16; }
      else { F = C ^ (B | ~D); g = (7 * i) % 16; }
      F = F + A + K[i] + M[g];
      A = D;
      D = C;
      C = B;
      B = B + ((F << S[i]) | (F >> (32 - S[i])));
    }
    a0 += A;
    b0 += B;
    c0 += C;
    d0 += D;
  }
  static const char* hex = "0123456789abcdef";
  std::string out;
  for (uint32_t v : {a0, b0, c0, d0})
    for (int i = 0; i < 4; ++i) {
      unsigned char b = (unsigned char)((v >> (8 * i)) & 0xff);
      out += hex[b >> 4];
      out += hex[b & 15];
    }
  return out;
}

}
