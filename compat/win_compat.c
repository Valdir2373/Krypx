

#include "win_compat.h"
#include <drivers/vga.h>

void win_compat_init(void) {
    
}

bool win_compat_load(const uint8_t *data, size_t size) {
    (void)data; (void)size;
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    vga_puts("[WIN32] Ambiente Windows PE detectado.\n");
    vga_puts("        Suporte PE em desenvolvimento — use ELF Linux por enquanto.\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    return false;
}
