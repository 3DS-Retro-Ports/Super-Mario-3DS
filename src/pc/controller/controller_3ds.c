#include "sm64.h"
#include "platform/3ds/platform.h"

s32 osContInit(OSMesgQueue *mq, u8 *bits, OSContStatus *status) {
    (void)mq;
    *bits = 1;
    status[0].errnum = 0;
    status[0].type = CONT_ABSOLUTE;
    return 0;
}
s32 osContStartReadData(OSMesgQueue *mq) { (void)mq; return 0; }
void osContGetReadData(OSContPad *pad) {
    struct PlatformInput input;
    platform_poll_input(&input);
    pad->button = input.buttons;
    pad->stick_x = input.x;
    pad->stick_y = input.y;
    pad->errnum = 0;
}
