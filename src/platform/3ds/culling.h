#ifndef SM64_3DS_CULLING_H
#define SM64_3DS_CULLING_H
#include <stdbool.h>
/* Port convention: one metre is 100 world units. Entity origins use a 3D sphere. */
#define ENTITY_RENDER_RADIUS_3DS 5000.0f
struct CullingFrustum3DS {
    float tan_x, tan_y, normal_x, normal_y, near_plane, far_plane;
    float roll_sin, roll_cos;
};
bool culling_3ds_in_range(const float position[3], const float mario[3]);
void culling_3ds_set_frustum(struct CullingFrustum3DS *f, float fov_degrees,
                           float aspect, float near_plane, float far_plane);
bool culling_3ds_sphere_visible(const struct CullingFrustum3DS *f,
                               const float camera_position[3], float radius, const float scale[3]);
#endif
