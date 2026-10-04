#ifndef SM64_3DS_SAVE_STORE_H
#define SM64_3DS_SAVE_STORE_H
#include <stddef.h>
#include <stdbool.h>
#define SAVE_STORE_DIRECTORY "sdmc:/3ds/sm64/saves"
/* Payload is the engine's 512-byte SaveBuffer: four slots, two copies each. */
bool save_store_init(void);
int save_store_read(unsigned offset, void *data, size_t size);
int save_store_write(unsigned offset, const void *data, size_t size);
int save_store_error(void);
#endif
