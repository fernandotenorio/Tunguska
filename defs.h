#ifndef DEFS_H
#define DEFS_H

#include <assert.h>
#include <inttypes.h>

#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanForward64)
#pragma intrinsic(_BitScanReverse64)
#endif

typedef uint64_t U64;
typedef uint32_t U32;
const U64 debruijn64 = U64(0x03f79d71b4cb0a89);

const int index64_trail[64] = {
    0,  1, 48,  2, 57, 49, 28,  3,
   61, 58, 50, 42, 38, 29, 17,  4,
   62, 55, 59, 36, 53, 51, 43, 22,
   45, 39, 33, 30, 24, 18, 12,  5,
   63, 47, 56, 27, 60, 41, 37, 16,
   54, 35, 52, 21, 44, 32, 23, 11,
   46, 26, 40, 15, 34, 20, 31, 10,
   25, 14, 19,  9, 13,  8,  7,  6
};

const int index64_lead[64] = {
    0, 47,  1, 56, 48, 27,  2, 60,
   57, 49, 41, 37, 28, 16,  3, 61,
   54, 58, 35, 52, 50, 42, 21, 44,
   38, 32, 29, 23, 17, 11,  4, 62,
   46, 55, 26, 59, 40, 36, 15, 53,
   34, 51, 20, 43, 31, 22, 10, 45,
   25, 39, 14, 33, 19, 30,  9, 24,
   13, 18,  8, 12,  7,  6,  5, 63
};

//inline or duplicated symbol error
inline int numberOfTrailingZeros(U64 bb){
   assert (bb != 0);
#ifdef _MSC_VER
   unsigned long index;
   _BitScanForward64(&index, bb);
   return index;
#else
  return __builtin_ctzll(bb);
#endif
}

inline int numberOfLeadingZeros(U64 bb){
   assert (bb != 0);
#ifdef _MSC_VER
   unsigned long index;
   _BitScanReverse64(&index, bb);
   return index; // _BitScanReverse64 returns the index of MSB, which is equivalent to 63 - clz
#else
   return 63 - __builtin_clzll(bb);
#endif
}

#endif