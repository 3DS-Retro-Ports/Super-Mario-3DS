#ifndef SM64_GFX_CULLING_H
#define SM64_GFX_CULLING_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Homogeneous signed area: equivalent to perspective divide plus the original
 * behind-eye sign correction, with no divisions (including when w == 0). */
static inline float gfx_triangle_winding(float ax, float ay, float aw,
                                         float bx, float by, float bw,
                                         float cx, float cy, float cw) {
    return ax * (cy*bw - by*cw) + ay * (bx*cw - cx*bw) + aw * (cx*by - bx*cy);
}
/* The endpoints are inclusive in F3DEX; original Fast3D encodes an exclusive
 * end in units of 40 bytes. A common outside plane rejects the current list. */
static inline bool gfx_cull_display_list(uint32_t w0, uint32_t w1, bool f3dex,
                                         const uint8_t *clip_flags, size_t stride, size_t capacity) {
    size_t first = (w0 & 0xffff) / (f3dex ? 2 : 40);
    size_t end = (w1 & 0xffff) / (f3dex ? 2 : 40);
    if (f3dex) ++end;
    if (first >= end || end > capacity) return false;
    uint8_t outside = 0x3f;
    for (size_t i = first; i < end; ++i) {
        outside &= clip_flags[i * stride];
        if (!outside) return false;
    }
    return true;
}
#endif
