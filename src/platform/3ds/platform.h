#ifndef SM64_3DS_PLATFORM_H
#define SM64_3DS_PLATFORM_H
#include <stdbool.h>
#include <stdint.h>
/* This header deliberately contains no libctru or libultra types. */
struct PlatformInput { uint16_t buttons; int8_t x, y; float camera_x, camera_y; };
uint64_t platform_time_us(void);
bool platform_init(void);
void platform_shutdown(void);
bool platform_is_new_3ds(void);
void platform_poll_input(struct PlatformInput *input);
void platform_run_job(void (*job)(void *), void *arg);
void platform_camera_delta(float *x, float *y);
#endif
