#ifndef SM64_3DS_CAMERA_H
#define SM64_3DS_CAMERA_H
#include <stdbool.h>
struct Camera;
void camera_3ds_update(struct Camera *camera, bool reset);
void camera_3ds_enable(bool value);
void camera_3ds_collision(bool value);
bool camera_3ds_enabled(void);
bool camera_3ds_has_collision(void);
void camera_3ds_rotate(float yaw, float pitch, float distance);
#endif
