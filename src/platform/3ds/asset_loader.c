#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "asset_loader.h"
#include "diagnostics.h"

static uint32_t read_be32(const uint8_t *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16)
        | ((uint32_t)bytes[2] << 8) | bytes[3];
}
const char *asset_validate_rom_header(const uint8_t *header, size_t header_size, size_t rom_size) {
    if (!header || header_size < SM64_ROM_HEADER_SIZE) return "ROM header truncated (need 0x40 bytes)";
    if (read_be32(header) != 0x80371240) return "ROM byte order at 0x00 is not big-endian .z64";
    if (rom_size != SM64_US_ROM_SIZE) return "USA ROM size must be exactly 8388608 bytes";
    if (memcmp(header + 0x20, "SUPER MARIO 64      ", 20)) return "ROM title mismatch at 0x20";
    if (memcmp(header + 0x3b, "NSME", 4)) return "ROM cartridge ID at 0x3B must be NSME (USA)";
    if (header[0x3f] != 0) return "ROM revision at 0x3F must be 0";
    if (read_be32(header + 0x10) != 0x635a2bff || read_be32(header + 0x14) != 0x8b022326)
        return "ROM CRC fields at 0x10/0x14 do not match USA revision 0";
    return NULL;
}
void asset_inspect_rom(void) {
    diagnostics_log("ROM inspection: %s", SM64_ROM_PATH);
    FILE *file = fopen(SM64_ROM_PATH, "rb");
    if (!file) {
        int code = errno;
        diagnostics_errno("fopen " SM64_ROM_PATH, code);
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        int code = errno; fclose(file);
        diagnostics_errno("ROM seek to end", code);
    }
    long size = ftell(file);
    if (size < 0) {
        int code = errno; fclose(file);
        diagnostics_errno("ROM ftell", code);
    }
    if (fseek(file, 0x00, SEEK_SET) != 0) {
        int code = errno; fclose(file);
        diagnostics_errno("ROM seek to header offset 0x00", code);
    }
    uint8_t header[SM64_ROM_HEADER_SIZE];
    size_t count = fread(header, 1, sizeof(header), file);
    if (ferror(file)) {
        int code = errno; fclose(file);
        diagnostics_errno("ROM header fread", code);
    }
    if (fclose(file) != 0) diagnostics_errno("ROM fclose", errno);
    diagnostics_log("ROM size: %ld bytes; header read: %u", size, (unsigned)count);
    if (count == sizeof(header)) {
        diagnostics_log("Magic@00=%08lX CRC@10/14=%08lX/%08lX",
            (unsigned long)read_be32(header), (unsigned long)read_be32(header + 0x10),
            (unsigned long)read_be32(header + 0x14));
        diagnostics_log("Title@20=%.20s ID@3B=%.4s rev@3F=%u", header + 0x20, header + 0x3b, header[0x3f]);
    }
    const char *error = asset_validate_rom_header(header, count, (size_t)size);
    if (error) diagnostics_fatal(error);
    diagnostics_log("ROM header valid: USA revision 0");
}
