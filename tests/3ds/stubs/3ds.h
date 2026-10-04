/* Failure-injection SDK subset. Only used by test_diagnostics.c. */
#ifndef TEST_3DS_H
#define TEST_3DS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t u32;
typedef int32_t Result;
#define R_FAILED(r) ((r) < 0)
#define R_SUCCEEDED(r) ((r) >= 0)
#define GFX_BOTTOM 1
#define GFX_LEFT 0
#define KEY_START 1
#define KEY_DUP 2
#define KEY_DLEFT 4
#define KEY_DDOWN 8
#define KEY_DRIGHT 16
static const char *failure;
static int console_calls, gpu_frames, frames, c3d_finis, c2d_finis;
static bool fail(const char *stage) { return !strcmp(failure, stage); }
static Result srvInit(void) { return 0; }
static Result aptInit(void) { return 0; }
static Result hidInit(void) { return fail("hid") ? (Result)0xD9000001 : 0; }
static Result fsInit(void) { return fail("fs") ? (Result)0xD9000002 : 0; }
static Result archiveMountSdmc(void) { return fail("sd") ? (Result)0xD9000003 : 0; }
static Result fontEnsureMapped(void) { return fail("font") ? (Result)0xD9000005 : 0; }
static Result gspInit(void) { return fail("gsp") ? (Result)0xD9000004 : 0; }
static void srvExit(void) {}
static void aptExit(void) {}
static void hidExit(void) {}
static void fsExit(void) {}
static void archiveUnmountAll(void) {}
static void gspExit(void) {}
static void gfxInitDefault(void) {}
static void gfxExit(void) {}
static void gfxSet3D(bool b) { (void)b; }
static void *gfxGetFramebuffer(int s, int eye, void *w, void *h) {
    (void)s; (void)eye; (void)w; (void)h; return (void *)1;
}
static void svcOutputDebugString(const char *s, size_t n) { (void)s; (void)n; }
static void consoleInit(int screen, void *p) { (void)p; if (screen != GFX_BOTTOM) __builtin_trap(); ++console_calls; }
static void consoleClear(void) {}
static bool aptMainLoop(void) { return frames++ < 3; }
static void hidScanInput(void) {}
static u32 hidKeysDown(void) { return 0; }
static void gfxFlushBuffers(void) {}
static void gspWaitForVBlank(void) {}
#endif
