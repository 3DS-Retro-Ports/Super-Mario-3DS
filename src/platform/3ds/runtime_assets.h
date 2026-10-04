#ifndef SM64_RUNTIME_ASSETS_H
#define SM64_RUNTIME_ASSETS_H
#include <stddef.h>
#include <stdint.h>
#include "runtime_asset_registry.h"
void runtime_assets_load(void);
int runtime_mio0(const uint8_t *input, size_t size, uint8_t *output, size_t capacity, size_t *written);
void runtime_sha1(const uint8_t *data, size_t size, uint8_t digest[20]);
void runtime_audio_convert(uint8_t *rom, size_t size);
#endif
