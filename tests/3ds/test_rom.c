#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform/3ds/asset_loader.h"
static char log_text[8192];
void diagnostics_log(const char *format, ...) {
    va_list args; va_start(args, format);
    size_t used = strlen(log_text);
    vsnprintf(log_text + used, sizeof(log_text) - used, format, args);
    va_end(args);
    strncat(log_text, "\n", sizeof(log_text) - strlen(log_text) - 1);
}
_Noreturn void diagnostics_fatal(const char *message) { fprintf(stderr, "%s\n%s", message, log_text); exit(2); }
_Noreturn void diagnostics_errno(const char *operation, int code) {
    fprintf(stderr, "%s: errno %d\n", operation, code); exit(3);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    if (!strcmp(argv[1], "inspect")) {
        asset_inspect_rom();
        puts(log_text);
        return 0;
    }
    FILE *file = fopen(argv[1], "rb"); assert(file);
    unsigned char header[SM64_ROM_HEADER_SIZE], copy[SM64_ROM_HEADER_SIZE];
    assert(fread(header, 1, sizeof(header), file) == sizeof(header)); fclose(file);
    assert(!asset_validate_rom_header(header, sizeof(header), SM64_US_ROM_SIZE));
    assert(strstr(asset_validate_rom_header(NULL, 0, 0), "truncated"));
    assert(strstr(asset_validate_rom_header(header, 63, SM64_US_ROM_SIZE), "truncated"));
    assert(strstr(asset_validate_rom_header(header, 64, SM64_US_ROM_SIZE - 1), "size"));
    const int offsets[] = {0, 0x20, 0x3b, 0x3e, 0x3f, 0x10, 0x14};
    const char *messages[] = {"byte order", "title", "cartridge ID", "USA", "revision", "CRC", "CRC"};
    for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        memcpy(copy, header, sizeof(copy)); copy[offsets[i]] ^= 1;
        const char *error = asset_validate_rom_header(copy, sizeof(copy), SM64_US_ROM_SIZE);
        assert(error && strstr(error, messages[i]));
    }
    puts("ROM size/header/offset tests passed");
    return 0;
}
