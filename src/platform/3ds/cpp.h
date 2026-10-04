#ifndef SM64_CPP_H
#define SM64_CPP_H
#include <3ds.h>
bool cpp_init(void);
void cpp_shutdown(void);
bool cpp_connected(void);
void cpp_poll(circlePosition *position, u32 *buttons);
#endif
