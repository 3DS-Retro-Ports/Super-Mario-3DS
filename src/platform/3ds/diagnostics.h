#ifndef SM64_3DS_DIAGNOSTICS_H
#define SM64_3DS_DIAGNOSTICS_H
#include <stdbool.h>
#include <stdint.h>
/* Main-thread only. No libctru types: safe to include alongside libultra. */
void diagnostics_init(void);
void diagnostics_shutdown(void);
void diagnostics_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
void diagnostics_check_sd(void);
bool diagnostics_frame_begin(void);
void diagnostics_frame_end(void);
_Noreturn void diagnostics_fatal(const char *message);
_Noreturn void diagnostics_result(const char *operation, int32_t result);
_Noreturn void diagnostics_errno(const char *operation, int error);
/* Interactive harness: force the software fallback after successful startup. */
_Noreturn void diagnostics_test_console(void);
#endif
