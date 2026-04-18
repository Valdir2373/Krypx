
#ifndef _FRAMEBUFFER_H
#define _FRAMEBUFFER_H

#include <types.h>
#include <multiboot.h>


#define COLOR_BLACK        0x00000000
#define COLOR_WHITE        0x00FFFFFF
#define COLOR_RED          0x00FF0000
#define COLOR_GREEN        0x0000FF00
#define COLOR_BLUE         0x000000FF
#define COLOR_CYAN         0x0000FFFF
#define COLOR_YELLOW       0x00FFFF00
#define COLOR_MAGENTA      0x00FF00FF
#define COLOR_DARK_GREY    0x00333333
#define COLOR_LIGHT_GREY   0x00AAAAAA
#define COLOR_ORANGE       0x00FF8800
#define COLOR_TRANSPARENT  0xFF000000   


#define KRYPX_BG           0x001E272E
#define KRYPX_TITLEBAR     0x002E86DE
#define KRYPX_TITLEBAR_IN  0x00636E72
#define KRYPX_ACCENT       0x0000CEC9
#define KRYPX_BUTTON       0x000984E3
#define KRYPX_TEXT         0x00DFE6E9
#define KRYPX_DESKTOP      0x000C2461
#define KRYPX_TASKBAR      0x001E272E

typedef struct {
    uint32_t *vram;        
    uint32_t *backbuf;     
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;       
    uint8_t   bpp;         
    bool      ready;
} framebuffer_t;

extern framebuffer_t fb;


bool fb_init(multiboot_info_t *mbi);


bool fb_is_ready(void);


static inline void fb_putpixel(int x, int y, uint32_t color) {
    if ((uint32_t)x >= fb.width || (uint32_t)y >= fb.height) return;
    fb.backbuf[y * (fb.pitch / 4) + x] = color;
}


static inline uint32_t fb_getpixel(int x, int y) {
    if ((uint32_t)x >= fb.width || (uint32_t)y >= fb.height) return 0;
    return fb.backbuf[y * (fb.pitch / 4) + x];
}


void fb_fill_rect(int x, int y, int w, int h, uint32_t color);


void fb_blit(int x, int y, int w, int h, const uint32_t *src);


void fb_clear(uint32_t color);


void fb_swap(void);


void fb_swap_region(int x, int y, int w, int h);


void fb_draw_rect(int x, int y, int w, int h, uint32_t color);

#endif 
