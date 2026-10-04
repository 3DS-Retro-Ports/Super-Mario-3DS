#include <3ds.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "platform.h"
#include "cpp.h"
#include "diagnostics.h"
#include "asset_loader.h"
#include "runtime_assets.h"

uint64_t platform_time_us(void) {
    u64 ticks = svcGetSystemTick();
    return (ticks / SYSCLOCK_ARM11) * 1000000ULL
        + (ticks % SYSCLOCK_ARM11) * 1000000ULL / SYSCLOCK_ARM11;
}

static bool new_model, ir_ready, cpp_ready, stopping;
static Thread worker;
static LightEvent work_ready, work_done;
static void (*work_function)(void *);
static void *work_argument;
static float camera_x, camera_y;
static bool touching;
static touchPosition previous_touch;

static void worker_main(void *unused) {
    (void)unused;
    for (;;) {
        LightEvent_Wait(&work_ready);
        if (stopping) break;
        work_function(work_argument);
        LightEvent_Signal(&work_done);
    }
}

/* Joined phases: the mutable SM64 object and collision state has one owner.
 * Caller must be core 0; jobs cannot recursively submit other jobs. */
void platform_run_job(void (*job)(void *), void *arg) {
    if (!worker) { job(arg); return; }
    work_function = job;
    work_argument = arg;
    LightEvent_Signal(&work_ready);
    LightEvent_Wait(&work_done);
}


bool platform_init(void) {
    diagnostics_check_sd();
    if (mkdir("mods", 0777) != 0 && errno != EEXIST) diagnostics_errno("mkdir mods", errno);
    runtime_assets_load();
    if (R_FAILED(APT_CheckNew3DS(&new_model))) new_model = false;
    if (new_model) {
        osSetSpeedupEnable(true);
        /* libctru's API is APT_SetAppCpuTimeLimit (core 1 is shared with OS). */
        if (R_SUCCEEDED(APT_SetAppCpuTimeLimit(30))) {
            LightEvent_Init(&work_ready, RESET_ONESHOT);
            LightEvent_Init(&work_done, RESET_ONESHOT);
            worker = threadCreate(worker_main, NULL, 128 * 1024, 0x30, 1, false);
            if (!worker) APT_SetAppCpuTimeLimit(0);
        }
    }
    ir_ready = new_model && R_SUCCEEDED(irrstInit());
    cpp_ready = !new_model && cpp_init();
    return true;
}

void platform_shutdown(void) {
    if (worker) {
        stopping = true;
        LightEvent_Signal(&work_ready);
        threadJoin(worker, U64_MAX);
        threadFree(worker);
        worker = NULL;
        APT_SetAppCpuTimeLimit(0);
    }
    if (ir_ready) irrstExit();
    if (cpp_ready) cpp_shutdown();
}
bool platform_is_new_3ds(void) { return new_model; }

static int stick_axis(int value) {
    if (value > -12 && value < 12) return 0;
    value = value * 80 / 156;
    return value < -80 ? -80 : value > 80 ? 80 : value;
}

void platform_poll_input(struct PlatformInput *input) {
    memset(input, 0, sizeof(*input));
    hidScanInput();
    u32 held = hidKeysHeld();
    circlePosition left = {0}, right = {0};
    hidCircleRead(&left);
    if (ir_ready) {
        irrstScanInput();
        held |= irrstKeysHeld();
        irrstCstickRead(&right);
    }
    if (cpp_ready) cpp_poll(&right, &held);
    /* N64 bits, kept here to avoid mixing libctru/libultra headers. */
    if (held & KEY_B) input->buttons |= 0x8000;
    if (held & KEY_Y) input->buttons |= 0x4000;
    if (held & (KEY_L | KEY_R)) input->buttons |= 0x2000;
    if (held & KEY_START) input->buttons |= 0x1000;
    if (held & KEY_ZL) input->buttons |= 0x0020;
    if (held & KEY_ZR) input->buttons |= 0x0010;
    if (held & KEY_DUP) input->buttons |= 0x0008;
    if (held & KEY_DDOWN) input->buttons |= 0x0004;
    if (held & KEY_DLEFT) input->buttons |= 0x0002;
    if (held & KEY_DRIGHT) input->buttons |= 0x0001;
    input->x = stick_axis(left.dx);
    input->y = stick_axis(left.dy);
    camera_x = stick_axis(right.dx) * 0.00075f;
    camera_y = stick_axis(right.dy) * 0.00075f;
    /* Select one camera source; reconnecting CPP resets the drag baseline. */
    bool analog_camera = ir_ready || (cpp_ready && cpp_connected());
    bool down = !analog_camera && (held & KEY_TOUCH) != 0;
    if (down) {
        touchPosition touch;
        hidTouchRead(&touch);
        if (touching) {
            camera_x += ((int)touch.px - previous_touch.px) * 0.008f;
            camera_y -= ((int)touch.py - previous_touch.py) * 0.008f;
        }
        previous_touch = touch;
    }
    touching = down;
    input->camera_x = camera_x;
    input->camera_y = camera_y;
}
void platform_camera_delta(float *x, float *y) {
    *x = camera_x; *y = camera_y;
    camera_x = camera_y = 0;
}
