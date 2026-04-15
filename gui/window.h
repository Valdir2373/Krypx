/*
 * gui/window.h — Estrutura de janela e Window Manager Stacking
 * Suporta drag, resize, minimize, maximize, e controle de processo.
 */
#ifndef _WINDOW_H
#define _WINDOW_H

#include <types.h>

#define MAX_WINDOWS      32
#define TITLEBAR_HEIGHT  28
#define BORDER_WIDTH      2
#define BTN_SIZE         14   /* Botões fechar/minimizar/maximizar */
#define BTN_MARGIN        6
#define RESIZE_BORDER     6   /* Largura da borda de resize */

/* Flags de janela */
#define WIN_VISIBLE     (1<<0)
#define WIN_FOCUSED     (1<<1)
#define WIN_MINIMIZED   (1<<2)
#define WIN_MAXIMIZED   (1<<3)
#define WIN_RESIZABLE   (1<<4)
#define WIN_NO_TITLEBAR (1<<5)
#define WIN_TOPMOST     (1<<6)

/* Direções de resize */
#define RESIZE_NONE    0
#define RESIZE_LEFT    1
#define RESIZE_RIGHT   2
#define RESIZE_BOTTOM  3
#define RESIZE_BL      4   /* Bottom-Left */
#define RESIZE_BR      5   /* Bottom-Right */

typedef struct window {
    uint32_t id;
    char     title[128];
    int      x, y, w, h;                   /* Incluindo decorações */
    int      content_x, content_y, content_w, content_h;
    uint32_t flags;
    uint32_t bg_color;

    uint32_t *buf;   /* Buffer privado do conteúdo */

    /* Callbacks */
    void (*on_paint)(struct window *win);
    void (*on_close)(struct window *win);
    void (*on_keydown)(struct window *win, char key);

    /* Z-order (lista duplamente encadeada) */
    struct window *above;   /* NULL = topo da pilha */
    struct window *below;   /* NULL = fundo da pilha */

    /* Drag state */
    bool drag_active;
    int  drag_off_x, drag_off_y;

    /* Resize state */
    bool    resize_active;
    uint8_t resize_dir;    /* RESIZE_LEFT / RIGHT / BOTTOM / BL / BR */

    /* Estado salvo antes de maximizar (para restore) */
    int save_x, save_y, save_w, save_h;

    /* Processo associado (0 = nenhum / kernel) */
    uint32_t proc_pid;

    bool used;
} window_t;

/* Inicializa o Window Manager */
void wm_init(void);

/* Cria uma janela */
window_t *wm_create(const char *title, int x, int y, int w, int h, uint32_t flags);

/* Fecha e libera uma janela (mata processo se proc_pid != 0) */
void wm_close(window_t *win);

/* Traz para o topo da z-order */
void wm_focus(window_t *win);

/* Renderiza todas as janelas (backbuffer → tela) */
void wm_render(void);

/* Notifica eventos de mouse */
void wm_mouse_move(int x, int y);
void wm_mouse_down(int x, int y, uint8_t btn);
void wm_mouse_up(int x, int y, uint8_t btn);

/* Notifica tecla para janela focada */
void wm_key_event(char key);

/* Retorna janela sob ponto (x, y) */
window_t *wm_window_at(int x, int y);

/* Pede redraw */
void wm_invalidate(window_t *win);

/* Desenha botões de janelas minimizadas na taskbar.
 * Parâmetros: posição inicial (x,y), tamanho do botão (bw x bh), limite direito. */
void wm_draw_taskbar_entries(int x, int y, int bw, int bh, int max_x);

/* Verifica clique em botões da taskbar (janelas minimizadas).
 * Retorna true se tratou o clique. */
bool wm_taskbar_entry_click(int mx, int my, int x, int y, int bw, int bh, int max_x);

/* Retorna posição atual do cursor (para destaque de hover) */
int wm_mouse_x(void);
int wm_mouse_y(void);

#endif /* _WINDOW_H */
