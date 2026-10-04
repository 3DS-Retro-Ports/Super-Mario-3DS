#ifndef SM64_CAMERA_MOTION_H
#define SM64_CAMERA_MOTION_H
float camera_smooth(float current, float target, float rate, float dt);
float camera_smooth_angle(float current, float target, float rate, float dt);
float camera_limit_pitch(float pitch);
#endif
