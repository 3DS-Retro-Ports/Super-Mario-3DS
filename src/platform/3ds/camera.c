#include <math.h>
#include "sm64.h"
#include "game/camera.h"
#include "game/mario.h"
#include "game/level_update.h"
#include "game/object_list_processor.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "platform.h"
#include "camera.h"
#include "camera_motion.h"

static float yaw, pitch, target_yaw, target_pitch, distance = 700, target_distance = 700;
static bool enabled = true, collisions = true;
static uint64_t last_time;
static Vec3f smooth_focus;
void camera_3ds_enable(bool value) { enabled = value; }
void camera_3ds_collision(bool value) { collisions = value; }
bool camera_3ds_enabled(void) { return enabled; }
bool camera_3ds_has_collision(void) { return collisions; }
void camera_3ds_rotate(float y, float p, float radius) {
    target_yaw = remainderf(y, 6.28318530718f);
    target_pitch = camera_limit_pitch(p);
    target_distance = fmaxf(200, fminf(1400, radius));
}
static bool initialized;
struct CameraTrace { Vec3f focus, desired, safe; };
/* Conservative swept camera sphere. Native floor/wall queries include moving
 * surfaces; a joined worker phase keeps the surface partition stable. */
static void trace_camera(void *arg) {
    struct CameraTrace *trace = arg;
    s32 previous = gCheckingSurfaceCollisionsForCamera;
    gCheckingSurfaceCollisionsForCamera = TRUE;
    vec3f_copy(trace->safe, trace->focus);
    for (int step = 1; step <= 96; ++step) {
        Vec3f point;
        for (int i = 0; i < 3; ++i)
            point[i] = trace->focus[i] + (trace->desired[i] - trace->focus[i]) * (step / 96.f);
        struct WallCollisionData walls = {0};
        walls.x = point[0]; walls.y = point[1]; walls.z = point[2]; walls.radius = 30;
        struct Surface *surface;
        float floor = find_floor(point[0], point[1] + 30, point[2], &surface);
        float ceil = find_ceil(point[0], point[1] - 30, point[2], &surface);
        if (find_wall_collisions(&walls) || point[1] < floor + 30 || point[1] > ceil - 30) break;
        vec3f_copy(trace->safe, point);
    }
    gCheckingSurfaceCollisionsForCamera = previous;
}
void camera_3ds_update(struct Camera *camera, bool reset) {
    float dx, dy;
    platform_camera_delta(&dx, &dy);
    if (!enabled || camera->cutscene || camera->mode == CAMERA_MODE_INSIDE_CANNON || camera->mode == CAMERA_MODE_C_UP) {
        initialized = false;
        return;
    }
    uint64_t now = platform_time_us();
    float dt = last_time && now >= last_time ? (now - last_time) / 1000000.f : 1.f / 30;
    last_time = now;
    if (reset || !initialized || fabsf(gMarioStates[0].pos[0] - smooth_focus[0]) > 1500
        || fabsf(gMarioStates[0].pos[2] - smooth_focus[2]) > 1500) {
        float x = camera->pos[0] - camera->focus[0];
        float z = camera->pos[2] - camera->focus[2];
        yaw = atan2f(x, z);
        float horizontal = sqrtf(x*x + z*z);
        pitch = camera_limit_pitch(atan2f(camera->pos[1] - camera->focus[1], horizontal));
        target_yaw = yaw; target_pitch = pitch; distance = target_distance;
        vec3f_copy(smooth_focus, gMarioStates[0].pos); smooth_focus[1] += 120;
        initialized = true;
    }
    target_yaw = remainderf(target_yaw - dx, 6.28318530718f);
    target_pitch = camera_limit_pitch(target_pitch + dy);
    yaw = camera_smooth_angle(yaw, target_yaw, 14, dt);
    pitch = camera_smooth(pitch, target_pitch, 14, dt);
    struct CameraTrace trace;
    for (int i = 0; i < 3; ++i)
        smooth_focus[i] = camera_smooth(smooth_focus[i], gMarioStates[0].pos[i] + (i == 1 ? 120 : 0), 18, dt);
    vec3f_copy(trace.focus, smooth_focus);
    float radius = camera_smooth(distance, target_distance, 5, dt);
    trace.desired[0] = trace.focus[0] + sinf(yaw) * cosf(pitch) * radius;
    trace.desired[1] = trace.focus[1] + sinf(pitch) * radius;
    trace.desired[2] = trace.focus[2] + cosf(yaw) * cosf(pitch) * radius;
    if (collisions) platform_run_job(trace_camera, &trace);
    else vec3f_copy(trace.safe, trace.desired);
    float sx = trace.safe[0] - trace.focus[0], sy = trace.safe[1] - trace.focus[1], sz = trace.safe[2] - trace.focus[2];
    distance = sqrtf(sx*sx + sy*sy + sz*sz);
    vec3f_copy(camera->pos, trace.safe);
    vec3f_copy(camera->focus, trace.focus);
    vec3f_copy(gLakituState.pos, trace.safe);
    vec3f_copy(gLakituState.curPos, trace.safe);
    vec3f_copy(gLakituState.goalPos, trace.safe);
    vec3f_copy(gLakituState.focus, trace.focus);
    vec3f_copy(gLakituState.curFocus, trace.focus);
    vec3f_copy(gLakituState.goalFocus, trace.focus);
    camera->yaw = camera->nextYaw = atan2s(trace.safe[2] - trace.focus[2], trace.safe[0] - trace.focus[0]);
    gLakituState.yaw = gLakituState.nextYaw = camera->yaw;
}
