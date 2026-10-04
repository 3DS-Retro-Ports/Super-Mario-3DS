#ifndef SM64_RUNTIME_ASSET_REGISTRY_H
#define SM64_RUNTIME_ASSET_REGISTRY_H
struct RuntimeAsset { void *destination; unsigned int size, rom, offset, kind; void *base; };
#define REGISTER_ASSET(id, dest, size_, rom_, offset_, kind_, base_) \
 static const struct RuntimeAsset id __attribute__((used, section("sm64_assets"), aligned(sizeof(void*)))) = \
 { dest, size_, rom_, offset_, kind_, base_ }
#endif
