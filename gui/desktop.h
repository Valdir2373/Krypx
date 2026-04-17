
#ifndef _DESKTOP_H
#define _DESKTOP_H

#include <types.h>
#include <multiboot.h>


void desktop_init(void);
void desktop_run(void);
void desktop_render(void);

/* Load an image file (PNG/JPG/JPEG) as the desktop wallpaper */
void desktop_set_wallpaper(const char *path);

#endif 
