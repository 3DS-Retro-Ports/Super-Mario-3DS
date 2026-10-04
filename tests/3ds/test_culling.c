#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "platform/3ds/culling.h"
#include "pc/gfx/gfx_culling.h"
int main(void) {
    const float mario[3] = {100, -300, 200}, scale[3] = {1, 1, 1};
    float p[3] = {5100, -300, 200};
    assert(culling_3ds_in_range(p, mario));
    p[0] += 1; assert(!culling_3ds_in_range(p, mario));
    p[0] = 3100; p[1] = 3700; assert(culling_3ds_in_range(p, mario));
    p[2] += 10; assert(!culling_3ds_in_range(p, mario));
    p[0] = 100; p[1] = -5301; p[2] = 200;
    assert(!culling_3ds_in_range(p, mario)); /* spherical, not horizontal only */
    struct CullingFrustum3DS f;
    culling_3ds_set_frustum(&f, 90, 4.f/3, 100, 20000);
    p[0] = 1200; p[1] = 0; p[2] = -1000;
    assert(culling_3ds_sphere_visible(&f, p, 0, scale)); /* correct 4:3 edge */
    p[0] = 1400; assert(!culling_3ds_sphere_visible(&f, p, 0, scale));
    assert(culling_3ds_sphere_visible(&f, p, 50, scale)); /* sphere grazes side plane */
    p[0] = 0; p[1] = 1100;
    assert(!culling_3ds_sphere_visible(&f, p, 0, scale));
    p[1] = -1100; assert(!culling_3ds_sphere_visible(&f, p, 0, scale));
    assert(culling_3ds_sphere_visible(&f, p, 100, scale));
    const float large[3] = {1, -3, 2};
    assert(culling_3ds_sphere_visible(&f, p, 25, large));
    f.roll_sin = 1; f.roll_cos = 0;
    assert(culling_3ds_sphere_visible(&f, p, 0, scale)); /* rolled into wide axis */
    p[0] = 1200; p[1] = 0;
    assert(!culling_3ds_sphere_visible(&f, p, 0, scale));
    p[0] = 0; p[2] = -98; assert(!culling_3ds_sphere_visible(&f, p, 0, scale));
    p[2] = -100; assert(culling_3ds_sphere_visible(&f, p, 0, scale));
    p[2] = -20000; assert(culling_3ds_sphere_visible(&f, p, 0, scale));
    p[2] = -20002; assert(!culling_3ds_sphere_visible(&f, p, 0, scale));
    p[2] = 1000; assert(!culling_3ds_sphere_visible(&f, p, 300, scale));
    /* Validate clip-plane intersection with real strided vertex records. */
    struct { float pos[4]; uint8_t clip; } v[4] = {0};
    v[1].clip = 3; v[2].clip = 2; v[3].clip = 6;
    assert(gfx_cull_display_list(2, 6, true, &v[0].clip, sizeof(v[0]), 4));
    assert(gfx_cull_display_list(40, 160, false, &v[0].clip, sizeof(v[0]), 4));
    assert(!gfx_cull_display_list(0, 6, true, &v[0].clip, sizeof(v[0]), 4));
    v[3].clip = 4;
    assert(!gfx_cull_display_list(2, 6, true, &v[0].clip, sizeof(v[0]), 4));
    assert(!gfx_cull_display_list(6, 2, true, &v[0].clip, sizeof(v[0]), 4));
    assert(!gfx_cull_display_list(0, 8, true, &v[0].clip, sizeof(v[0]), 4));
    /* Compare optimized winding signs against perspective division for all
     * front/behind-eye combinations, avoiding zero-area roundoff ambiguities. */
    uint32_t seed = 1;
    for (int n = 0; n < 10000; ++n) {
        float a[9];
        for (int i = 0; i < 9; ++i) {
            seed = seed * 1664525u + 1013904223u;
            a[i] = (float)(int)(seed % 2001) - 1000;
        }
        for (int i = 2; i < 9; i += 3) if (a[i] == 0) a[i] = 1;
        double dx1 = (double)a[0]/a[2] - (double)a[3]/a[5];
        double dy1 = (double)a[1]/a[2] - (double)a[4]/a[5];
        double dx2 = (double)a[6]/a[8] - (double)a[3]/a[5];
        double dy2 = (double)a[7]/a[8] - (double)a[4]/a[5];
        double old = dx1*dy2 - dy1*dx2;
        if ((a[2]<0) ^ (a[5]<0) ^ (a[8]<0)) old = -old;
        float area = gfx_triangle_winding(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]);
        if (fabs(old) > .001) assert((area > 0) == (old > 0));
    }
    assert(isfinite(gfx_triangle_winding(0,0,0, 1,0,1, 0,1,1)));
    puts("50 m radius, six frustum planes, scale/roll, display-list clipping and winding: PASS");
}
