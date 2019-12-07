#if !defined(PACMAN_INTRINSICS_H)

#include "math.h"

inline s32
round_float_to_s32(f32 value) {
    s32 result = (s32)roundf(value);
    return result;
}

inline u32
round_float_to_u32(f32 value) {
    u32 result = (u32)roundf(value);
    return result;
}

#define PACMAN_INTRINSICS_H
#endif
