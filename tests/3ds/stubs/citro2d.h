#ifndef TEST_CITRO2D_H
#define TEST_CITRO2D_H
#include "3ds.h"
typedef int C3D_RenderTarget;
typedef void *C2D_TextBuf;
typedef struct { int unused; } C2D_Text;
#define C3D_FRAME_SYNCDRAW 0
#define C3D_DEFAULT_CMDBUF_SIZE 1
#define C2D_DEFAULT_MAX_OBJECTS 1
#define C2D_WithColor 1
#define GX_TRANSFER_FMT_RGBA8 0
#define GX_TRANSFER_FMT_RGB8 0
#define GX_TRANSFER_IN_FORMAT(x) (x)
#define GX_TRANSFER_OUT_FORMAT(x) (x)
static bool C3D_Init(int size) { (void)size; return !fail("c3d"); }
static bool C2D_Init(int size) { (void)size; return !fail("c2d"); }
static void C3D_Fini(void) { ++c3d_finis; }
static void C2D_Fini(void) { ++c2d_finis; }
static bool C3D_FrameBegin(int flags) { (void)flags; return !fail("frame"); }
static void C3D_FrameEnd(int flags) { (void)flags; }
static void C3D_FrameSync(void) {}
static C3D_RenderTarget *C2D_CreateScreenTarget(int screen, int eye) {
    (void)eye; if (screen != GFX_BOTTOM) __builtin_trap();
    return fail("target") ? NULL : (void *)1;
}
static void C3D_RenderTargetDelete(C3D_RenderTarget *t) { (void)t; }
static void C3D_RenderTargetSetOutput(C3D_RenderTarget *t, int screen, int eye, int flags) {
    (void)t; (void)eye; (void)flags; if (screen != GFX_BOTTOM) __builtin_trap();
}
static C2D_TextBuf C2D_TextBufNew(int size) { (void)size; return fail("text") ? NULL : (void *)1; }
static void C2D_TextBufDelete(C2D_TextBuf buf) { (void)buf; }
static void C2D_TextBufClear(C2D_TextBuf buf) { (void)buf; }
static void C2D_TextParse(C2D_Text *t, C2D_TextBuf b, const char *s) { (void)t; (void)b; (void)s; }
static void C2D_TextOptimize(C2D_Text *t) { (void)t; }
static void C2D_TextGetDimensions(C2D_Text *t, float x, float y, float *w, float *h) {
    (void)t; (void)x; (void)y; (void)h; *w = 100;
}
static void C2D_DrawText(C2D_Text *t, int flags, float x, float y, float z, float sx, float sy, u32 c) {
    (void)t; (void)flags; (void)x; (void)y; (void)z; (void)sx; (void)sy; (void)c;
}
static u32 C2D_Color32(int r, int g, int b, int a) { (void)r; (void)g; (void)b; (void)a; return 0; }
static void C2D_Prepare(void) { ++gpu_frames; }
static void C2D_TargetClear(C3D_RenderTarget *t, u32 c) { (void)t; (void)c; }
static void C2D_SceneBegin(C3D_RenderTarget *t) { (void)t; }
static void C2D_Flush(void) {}
#endif
