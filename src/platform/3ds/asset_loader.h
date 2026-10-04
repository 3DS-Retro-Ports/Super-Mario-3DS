#ifndef SM64_3DS_ASSET_LOADER_H
#define SM64_3DS_ASSET_LOADER_H
#include <stddef.h>
#include <stdint.h>
#ifndef SM64_ROM_PATH
#define SM64_ROM_PATH "sdmc:/sm64/baserom.us.z64"
#endif
#define SM64_US_ROM_SIZE 0x800000u
#define SM64_ROM_HEADER_SIZE 0x40u
/* Header checks shared with runtime_assets.c, which verifies SHA-1 and loads
 * asset ranges. N64 ROM offsets are never cast to native ARM pointers. */
const char *asset_validate_rom_header(const uint8_t *header, size_t header_size, size_t rom_size);
void asset_inspect_rom(void);
#endif
