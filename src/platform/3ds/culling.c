#include "culling.h"
#include <math.h>
bool culling_3ds_in_range(const float position[3], const float mario[3]) {
    float x = position[0] - mario[0], y = position[1] - mario[1], z = position[2] - mario[2];
    return x*x + y*y + z*z <= ENTITY_RENDER_RADIUS_3DS * ENTITY_RENDER_RADIUS_3DS;
}
void culling_3ds_set_frustum(struct CullingFrustum3DS *f, float fov_degrees,
                           float aspect, float near_plane, float far_plane) {
    /* Cache trig and plane normal lengths once per perspective, not per entity. */
    f->tan_y = tanf(fov_degrees * 0.00872664626f);
    f->tan_x = f->tan_y * aspect;
    f->normal_x = sqrtf(1 + f->tan_x * f->tan_x);
    f->normal_y = sqrtf(1 + f->tan_y * f->tan_y);
    f->near_plane = near_plane; f->far_plane = far_plane;
    f->roll_sin = 0; f->roll_cos = 1;
}
bool culling_3ds_sphere_visible(const struct CullingFrustum3DS *f,
                               const float p[3], float radius, const float scale[3]) {
    radius = fabsf(radius) * fmaxf(fabsf(scale[0]), fmaxf(fabsf(scale[1]), fabsf(scale[2])));
    /* One unit of slack absorbs fixed-point projection/rotation rounding. */
    radius += 1;
    float depth = -p[2];
    if (depth + radius < f->near_plane || depth - radius > f->far_plane) return false;
    float x = p[0] * f->roll_cos - p[1] * f->roll_sin;
    float y = p[0] * f->roll_sin + p[1] * f->roll_cos;
    return fabsf(x) <= depth * f->tan_x + radius * f->normal_x
        && fabsf(y) <= depth * f->tan_y + radius * f->normal_y;
}
