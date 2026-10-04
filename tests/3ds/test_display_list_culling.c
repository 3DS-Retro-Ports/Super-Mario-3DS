/* Exercise actual Fast3D dispatch, including returning from a culled child list. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <PR/mbi.h>
#include "pc/gfx/gfx_pc.c"
void platform_run_job(void (*job)(void *), void *arg) { job(arg); }
_Noreturn void diagnostics_fatal(const char *message) { fprintf(stderr, "%s\n", message); abort(); }
void diagnostics_log(const char *format, ...) {
    va_list args; va_start(args, format); vfprintf(stderr, format, args); va_end(args);
}
int main(void) {
    Gfx child[] = {
        gsSPCullDisplayList(1, 3),
        gsDPSetEnvColor(1, 2, 3, 4),
        gsSPEndDisplayList(),
    };
    Gfx parent[] = {
        gsSPDisplayList(child),
        gsDPSetPrimColor(0, 0, 5, 6, 7, 8),
        gsSPEndDisplayList(),
    };
    rsp.loaded_vertices[1].clip_rej = 3;
    rsp.loaded_vertices[2].clip_rej = 2;
    rsp.loaded_vertices[3].clip_rej = 6;
    gfx_run_dl(parent);
    assert(rdp.env_color.r == 0 && rdp.prim_color.r == 5);
    rsp.loaded_vertices[3].clip_rej = 4; /* no plane excludes all selected vertices */
    gfx_run_dl(parent);
    assert(rdp.env_color.r == 1 && rdp.env_color.a == 4 && rdp.prim_color.a == 8);
    puts("Actual F3DEX CULLDL skips child commands and resumes parent: PASS");
}
