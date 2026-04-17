
#ifndef _WINDOW_H
#define _WINDOW_H

#include <types.h>

#define MAX_WINDOWS      32
#define TITLEBAR_HEIGHT  28
#define BORDER_WIDTH      2
#define BTN_SIZE         14   
#define BTN_MARGIN        6
#define RESIZE_BORDER     6   


#define WIN_VISIBLE     (1<<0)
#define WIN_FOCUSED     (1<<1)
#define WIN_MINIMIZED   (1<<2)
#define WIN_MAXIMIZED   (1<<3)
#define WIN_RESIZABLE   (1<<4)
#define WIN_NO_TITLEBAR (1<<5)
#define WIN_TOPMOST     (1<<6)


#define RESIZE_NONE    0
#define RESIZE_LEFT    1
#define RESIZE_RIGHT   2
#define RESIZE_BOTTOM  3
#define RESIZE_BL      4   
#define RESIZE_BR      5   

typedef struct window {
    uint32_t id;
    char     title[128];
    int      x, y, w, h;                   
    int      content_x, content_y, content_w, content_h;
    uint32_t flags;
    uint32_t bg_color;

    uint32_t *buf;   

    
    void (*on_paint)(struct window *win);
    void (*on_close)(struct window *win);
    void (*on_keydown)(struct window *win, char key);
    void (*on_click)(struct window *win, int x, int y);

    
    struct window *above;   
    struct window *below;   

    
    bool drag_active;
    int  drag_off_x, drag_off_y;

    
    bool    resize_active;
    uint8_t resize_dir;    

    
    int save_x, save_y, save_w, save_h;

    
    uint32_t proc_pid;

    bool used;
} window_t;


void wm_init(void);


window_t *wm_create(const char *title, int x, int y, int w, int h, uint32_t flags);


void wm_close(window_t *win);


void wm_focus(window_t *win);


void wm_render(void);


void wm_mouse_move(int x, int y);
void wm_mouse_down(int x, int y, uint8_t btn);
void wm_mouse_up(int x, int y, uint8_t btn);


void wm_key_event(char key);


window_t *wm_window_at(int x, int y);


void wm_invalidate(window_t *win);


void wm_draw_taskbar_entries(int x, int y, int bw, int bh, int max_x);


bool wm_taskbar_entry_click(int mx, int my, int x, int y, int bw, int bh, int max_x);


int wm_mouse_x(void);
int wm_mouse_y(void);

#endif 
