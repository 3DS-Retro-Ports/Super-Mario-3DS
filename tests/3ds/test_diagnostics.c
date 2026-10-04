#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static jmp_buf stopped;
static int exit_code;
_Noreturn static void test_exit(int code) { exit_code = code; longjmp(stopped, 1); }
#define exit test_exit
#include "../../src/platform/3ds/startup.c"
#include "../../src/platform/3ds/diagnostics.c"
#undef exit
static bool logged(const char *needle) {
    for (unsigned i = 0; i < line_count; ++i)
        if (strstr(lines[i], needle)) return true;
    return false;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    failure = argv[1];
    if (!strcmp(failure, "log")) {
        char oversized[1024]; memset(oversized, 'x', sizeof(oversized)); oversized[1023] = 0;
        diagnostics_log("%s", oversized);
        assert(logged("[truncated]"));
        for (int i = 0; i < 200; ++i) diagnostics_log("row %d", i);
        assert(line_count == LOG_ROWS && logged("row 199") && !logged("row 0"));
        diagnostics_log("escape\033[2J");
        assert(logged("escape?[2J"));
        for (unsigned i = 0; i < line_count; ++i) assert(strlen(lines[i]) <= LOG_COLS);
        return 0;
    }
    __appInit();
    if (!setjmp(stopped)) {
        diagnostics_init();
        if (fail("fs") || fail("sd")) diagnostics_check_sd();
        if (fail("errno")) diagnostics_errno("asset open", ENOENT);
        if (fail("console")) diagnostics_test_console();
        diagnostics_result("asset load", (int32_t)0xD9001234);
    }
    assert(exit_code == EXIT_FAILURE);
    if (fail("c3d") || fail("font") || fail("c2d") || fail("target") || fail("text") || fail("frame") || fail("console")) {
        assert(console_calls == 1 && gpu_frames == 0);
    } else if (fail("gsp")) {
        assert(console_calls == 0 && gpu_frames == 0);
        assert(logged("0xD9000004"));
    } else assert(console_calls == 0 && gpu_frames > 0);
    if (fail("font")) assert(logged("0xD9000005"));
    if (fail("sd")) assert(logged("0xD9000003"));
    if (fail("fs")) assert(logged("0xD9000002"));
    if (fail("hid")) assert(logged("0xD9000001"));
    if (fail("errno")) assert(logged("errno 2"));
    if (fail("ok")) assert(logged("0xD9001234"));
    assert(c3d_finis == (!fail("c3d") && !fail("gsp")));
    assert(c2d_finis == (!fail("c3d") && !fail("font") && !fail("c2d") && !fail("gsp")));
    __appExit();
    return 0;
}
