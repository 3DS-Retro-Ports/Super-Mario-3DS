#include <3ds.h>
#include <citro2d.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "diagnostics.h"
#include "startup.h"

#define LOG_ROWS 96
#define LOG_COLS 38
#define PAGE_ROWS 15
static char lines[LOG_ROWS][LOG_COLS + 1];
static unsigned line_count;
static bool gfx_ready, gsp_ready, c3d_ready, c2d_ready, frame_active;
static C3D_RenderTarget *error_target;
static C2D_TextBuf text_buffer;

static void append_line(const char *text, size_t length) {
    if (line_count == LOG_ROWS) {
        memmove(lines, lines + 1, sizeof(lines) - sizeof(lines[0]));
        --line_count;
    }
    memcpy(lines[line_count], text, length);
    lines[line_count++][length] = '\0';
}
void diagnostics_log(const char *format, ...) {
    char message[512];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    if (length < 0) return;
    if ((size_t)length >= sizeof(message))
        memcpy(message + sizeof(message) - 15, "...[truncated]", 15);
    const char *start = message;
    for (char *p = message;; ++p) {
        if (*p == '\0' || *p == '\n' || (size_t)(p - start) == LOG_COLS) {
            append_line(start, p - start);
            if (!*p) break;
            if (*p == '\n') { start = p + 1; continue; }
            start = p;
        }
        /* Never interpret terminal escapes from paths or Lua messages. */
        if ((unsigned char)*p < 32 || (unsigned char)*p > 126) *p = '?';
    }
}

static void save_log(void) {
    FILE *file = fopen("sdmc:/sm64/diagnostics.log", "w");
    if (!file) {
        int code = errno;
        diagnostics_log("Log file unavailable: errno %d (%s)", code, strerror(code));
        return;
    }
    bool failed = false;
    int code = 0;
    for (unsigned i = 0; i < line_count; ++i) {
        if (fprintf(file, "%s\n", lines[i]) < 0) { failed = true; code = errno; break; }
    }
    if (fclose(file) != 0 && !failed) { failed = true; code = errno; }
    if (failed) diagnostics_log("Log write failed: errno %d (%s)", code, strerror(code));
}

bool diagnostics_frame_begin(void) {
    frame_active = C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    return frame_active;
}
void diagnostics_frame_end(void) {
    if (frame_active) { C3D_FrameEnd(0); frame_active = false; }
}
void diagnostics_shutdown(void) {
    diagnostics_frame_end();
    if (c3d_ready) C3D_FrameSync();
    if (text_buffer) { C2D_TextBufDelete(text_buffer); text_buffer = NULL; }
    if (error_target) { C3D_RenderTargetDelete(error_target); error_target = NULL; }
    if (c2d_ready) { C2D_Fini(); c2d_ready = false; }
    if (c3d_ready) { C3D_Fini(); c3d_ready = false; }
    if (gfx_ready) { gfxExit(); gfx_ready = false; }
    if (gsp_ready) { gspExit(); gsp_ready = false; }
}
static void draw_text(const char *str, float y, u32 color) {
    C2D_Text text;
    C2D_TextParse(&text, text_buffer, str);
    C2D_TextOptimize(&text);
    float width;
    C2D_TextGetDimensions(&text, 0.4f, 0.4f, &width, NULL);
    float scale = width > 304 ? 0.4f * 304 / width : 0.4f;
    C2D_DrawText(&text, C2D_WithColor, 8, y, 0, scale, 0.4f, color);
}

_Noreturn void diagnostics_fatal(const char *message) {
    diagnostics_log("ERROR: %s", message);
    diagnostics_frame_end();
    if (c3d_ready) C3D_FrameSync();
    save_log();
    bool gpu = c2d_ready && error_target && text_buffer;
    if (!gfx_ready) {
        /* No working display service means neither Citro2D nor a framebuffer
         * can display text. Preserve the error in the debugger and exit. */
        for (unsigned i = 0; i < line_count; ++i)
            svcOutputDebugString(lines[i], strlen(lines[i]));
        diagnostics_shutdown();
        exit(EXIT_FAILURE);
    }
    if (gpu) C3D_RenderTargetSetOutput(error_target, GFX_BOTTOM, GFX_LEFT,
        GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |
        GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));
    else consoleInit(GFX_BOTTOM, NULL);
    unsigned pages = (line_count + PAGE_ROWS - 1) / PAGE_ROWS;
    unsigned page = pages - 1;
    bool redraw = true;
    while (aptMainLoop()) {
        if (R_SUCCEEDED(startup_hid)) {
            hidScanInput();
            u32 keys = hidKeysDown();
            if (keys & KEY_START) break;
            if ((keys & (KEY_DUP | KEY_DLEFT)) && page) { --page; redraw = true; }
            if ((keys & (KEY_DDOWN | KEY_DRIGHT)) && page + 1 < pages) { ++page; redraw = true; }
        }
        char footer[64];
        snprintf(footer, sizeof(footer), "D-pad: log %u/%u   START: exit", page + 1, pages);
        if (gpu) {
            if (!diagnostics_frame_begin()) {
                /* No usable GPU frame: avoid retrying forever. */
                gpu = false;
                consoleInit(GFX_BOTTOM, NULL);
                redraw = true;
                continue;
            }
            C2D_Prepare();
            C2D_TargetClear(error_target, C2D_Color32(18, 24, 36, 255));
            C2D_SceneBegin(error_target);
            C2D_TextBufClear(text_buffer);
            draw_text("SM64 / 3DS - stopped", 8, C2D_Color32(255, 110, 100, 255));
            for (unsigned i = 0; i < PAGE_ROWS && page * PAGE_ROWS + i < line_count; ++i)
                draw_text(lines[page * PAGE_ROWS + i], 30 + 12 * i, C2D_Color32(240, 240, 240, 255));
            draw_text(footer, 220, C2D_Color32(155, 190, 220, 255));
            C2D_Flush();
            diagnostics_frame_end();
        } else {
            if (redraw) {
                consoleClear();
                printf("SM64 / 3DS - stopped\nSoftware display fallback\n\n");
                for (unsigned i = 0; i < PAGE_ROWS && page * PAGE_ROWS + i < line_count; ++i)
                    printf("%s\n", lines[page * PAGE_ROWS + i]);
                printf("\n%s\n", footer);
                redraw = false;
            }
            gfxFlushBuffers();
            gspWaitForVBlank();
        }
        /* Without HID, show one frame and allow the process to exit cleanly. */
        if (R_FAILED(startup_hid)) break;
    }
    diagnostics_shutdown();
    exit(EXIT_FAILURE);
}
_Noreturn void diagnostics_result(const char *operation, int32_t result) {
    diagnostics_log("%s: Result 0x%08lX (%ld)", operation, (unsigned long)(uint32_t)result, (long)result);
    diagnostics_fatal(operation);
}
_Noreturn void diagnostics_errno(const char *operation, int error) {
    diagnostics_log("%s: errno %d (%s)", operation, error, strerror(error));
    diagnostics_fatal(operation);
}
void diagnostics_init(void) {
    diagnostics_log("Starting bottom screen diagnostics");
    if (R_FAILED(startup_srv)) diagnostics_result("srvInit", startup_srv);
    if (R_FAILED(startup_apt)) diagnostics_result("aptInit", startup_apt);
    Result result = gspInit();
    if (R_FAILED(result)) diagnostics_result("gspInit", result);
    gsp_ready = true;
    gfxInitDefault();
    gfx_ready = true;
    gfxSet3D(false);
    if (!gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL)) {
        gfx_ready = false;
        diagnostics_fatal("gfxGetFramebuffer returned NULL");
    }
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) diagnostics_fatal("C3D_Init returned false");
    c3d_ready = true;
    result = fontEnsureMapped();
    if (R_FAILED(result)) diagnostics_result("fontEnsureMapped", result);
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) diagnostics_fatal("C2D_Init returned false");
    c2d_ready = true;
    error_target = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if (!error_target) diagnostics_fatal("C2D_CreateScreenTarget returned NULL");
    text_buffer = C2D_TextBufNew(2048);
    if (!text_buffer) diagnostics_fatal("C2D_TextBufNew returned NULL");
    diagnostics_log("Citro3D / Citro2D ready (320x240)");
    if (R_FAILED(startup_hid)) diagnostics_result("hidInit", startup_hid);
}
void diagnostics_check_sd(void) {
    if (R_FAILED(startup_fs)) diagnostics_result("fsInit", startup_fs);
    if (R_FAILED(startup_sd)) diagnostics_result("archiveMountSdmc", startup_sd);
    diagnostics_log("SD mounted successfully (device %ld)", (long)startup_sd);
    if (mkdir("sdmc:/sm64", 0777) != 0 && errno != EEXIST)
        diagnostics_errno("mkdir sdmc:/sm64", errno);
    if (chdir("sdmc:/sm64") != 0) diagnostics_errno("chdir sdmc:/sm64", errno);
    diagnostics_log("Working directory: sdmc:/sm64/");
}
_Noreturn void diagnostics_test_console(void) {
    /* A runtime switch for testing the same fallback used by failed C2D_Init. */
    diagnostics_frame_end();
    C3D_FrameSync();
    C2D_TextBufDelete(text_buffer); text_buffer = NULL;
    C3D_RenderTargetDelete(error_target); error_target = NULL;
    C2D_Fini(); c2d_ready = false;
    C3D_Fini(); c3d_ready = false;
    diagnostics_fatal("SELF-TEST: software fallback (intentional)");
}
