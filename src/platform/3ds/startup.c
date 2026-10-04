#include <3ds.h>
#include "startup.h"

int32_t startup_srv, startup_apt, startup_hid, startup_fs, startup_sd;
void __appInit(void) {
    startup_srv = srvInit();
    if (R_FAILED(startup_srv)) {
        startup_apt = startup_hid = startup_fs = startup_sd = startup_srv;
        return;
    }
    startup_apt = aptInit();
    startup_hid = hidInit();
    startup_fs = fsInit();
    startup_sd = R_SUCCEEDED(startup_fs) ? archiveMountSdmc() : startup_fs;
}
void __appExit(void) {
    if (R_SUCCEEDED(startup_sd)) archiveUnmountAll();
    if (R_SUCCEEDED(startup_fs)) fsExit();
    if (R_SUCCEEDED(startup_hid)) hidExit();
    if (R_SUCCEEDED(startup_apt)) aptExit();
    if (R_SUCCEEDED(startup_srv)) srvExit();
}
