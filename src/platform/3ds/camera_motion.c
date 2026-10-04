#include <math.h>
#include "camera_motion.h"
float camera_smooth(float current, float target, float rate, float dt) {
    return current + (target - current) * (1.f - expf(-rate * fmaxf(0, fminf(dt, .1f))));
}
float camera_smooth_angle(float current, float target, float rate, float dt) {
    return remainderf(camera_smooth(current, current + remainderf(target - current, 6.28318530718f), rate, dt), 6.28318530718f);
}
float camera_limit_pitch(float pitch) { return fminf(1.2f, fmaxf(-.45f, pitch)); }
