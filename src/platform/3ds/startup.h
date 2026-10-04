#ifndef SM64_3DS_STARTUP_H
#define SM64_3DS_STARTUP_H
#include <stdint.h>
/* __appInit captures the original mount Result instead of retrying an already
 * mounted device (which would hide the original error). */
extern int32_t startup_srv, startup_apt, startup_hid, startup_fs, startup_sd;
#endif
