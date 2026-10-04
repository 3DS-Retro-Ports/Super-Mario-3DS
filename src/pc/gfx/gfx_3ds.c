#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include "gfx_3ds.h"
#include "platform/3ds/platform.h"
#include "platform/3ds/diagnostics.h"

static C3D_RenderTarget *target;
Gfx3DSMode gGfx3DSMode = GFX_3DS_MODE_NORMAL;

static void init(void) {
    diagnostics_init();
    consoleInit(GFX_TOP, NULL);
    if (!platform_init()) diagnostics_fatal("platform_init returned false");
    /* Physical framebuffer orientation is 240 x 320. Logical view is 320 x 240. */
    target = C3D_RenderTargetCreate(240, 320, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (!target) diagnostics_fatal("Gameplay C3D_RenderTargetCreate returned NULL");
    C3D_RenderTargetSetOutput(target, GFX_BOTTOM, GFX_LEFT,
        GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
        GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    /* Original game simulation is 30 Hz; 60 Hz would double game and audio speed. */
    C3D_FrameRate(30.0f);
}
static void loop(void (*frame)(void)) {
    while (aptMainLoop()) frame();
}
void gfx_3ds_shutdown(void) {
    platform_shutdown();
    C3D_RenderTargetDelete(target);
    diagnostics_shutdown();
}
static void dimensions(uint32_t *w, uint32_t *h) { *w = 320; *h = 240; }
static void events(void) {}
static bool begin(void) {
    if (!diagnostics_frame_begin()) diagnostics_fatal("C3D_FrameBegin returned false");
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x000000ff, 0xffffffff);
    if (!C3D_FrameDrawOn(target)) diagnostics_fatal("C3D_FrameDrawOn returned false");
    return true;
}
static void swap_begin(void) { diagnostics_frame_end(); }
static void swap_end(void) {}
static double now(void) { return svcGetSystemTick() / (double)SYSCLOCK_ARM11; }
struct GfxWindowManagerAPI gfx_3ds = { init, loop, dimensions, events, begin, swap_begin, swap_end, now };
