/*
 * drivers/keyboard.c — Driver teclado PS/2 com scancode Set 1
 * Converte scancodes para ASCII. Suporta Shift e Caps Lock.
 * Usa um ring buffer de 256 bytes para não perder teclas.
 */

#include <drivers/keyboard.h>
#include <kernel/idt.h>
#include <types.h>
#include <io.h>

/* Tamanho do buffer circular */
#define KB_BUFFER_SIZE 256

/* Buffer circular */
static char     kb_buf[KB_BUFFER_SIZE];
static uint8_t  kb_read_pos  = 0;
static uint8_t  kb_write_pos = 0;

/* Estado das teclas modificadoras */
static bool shift_pressed  = false;
static bool caps_lock      = false;
static bool ctrl_pressed   = false;
static bool alt_pressed    = false;

/* Tabela de scancodes Set 1 → ASCII (sem shift) */
static const char sc_to_ascii[128] = {
    0,    27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,    '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*',  0,   ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* F1-F10 */
    0, 0,                            /* Num Lock, Scroll Lock */
    0, 0, 0, '-',                    /* Home, Up, PgUp, - */
    0, 0, 0, '+',                    /* Left, 5, Right, + */
    0, 0, 0, 0, 0,                   /* End, Down, PgDn, Ins, Del */
    0, 0, 0,                         /* ? */
    0, 0,                            /* F11, F12 */
};

/* Tabela de scancodes Set 1 → ASCII (com shift) */
static const char sc_to_ascii_shift[128] = {
    0,    27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*',  0,   ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0,
    0, 0, 0, '-',
    0, 0, 0, '+',
    0, 0, 0, 0, 0,
    0, 0, 0,
    0, 0,
};

/* Scancodes especiais */
#define SC_LSHIFT      0x2A
#define SC_RSHIFT      0x36
#define SC_LCTRL       0x1D
#define SC_LALT        0x38
#define SC_CAPS_LOCK   0x3A
#define SC_RELEASE     0x80  /* Bit 7 = tecla solta */

static void kb_buf_push(char c) {
    uint8_t next = (kb_write_pos + 1) % KB_BUFFER_SIZE;
    if (next != kb_read_pos) {  /* Não sobrescreve se cheio */
        kb_buf[kb_write_pos] = c;
        kb_write_pos = next;
    }
}

static void keyboard_handler(registers_t *regs) {
    (void)regs;
    uint8_t sc = inb(KB_DATA_PORT);

    /* Tecla solta (bit 7 = 1) */
    if (sc & SC_RELEASE) {
        uint8_t released = sc & ~SC_RELEASE;
        if (released == SC_LSHIFT || released == SC_RSHIFT) shift_pressed = false;
        if (released == SC_LCTRL)  ctrl_pressed = false;
        if (released == SC_LALT)   alt_pressed  = false;
        pic_send_eoi(33);
        return;
    }

    /* Teclas modificadoras */
    if (sc == SC_LSHIFT || sc == SC_RSHIFT) { shift_pressed = true;  goto eoi; }
    if (sc == SC_LCTRL)                     { ctrl_pressed  = true;  goto eoi; }
    if (sc == SC_LALT)                      { alt_pressed   = true;  goto eoi; }
    if (sc == SC_CAPS_LOCK)                 { caps_lock = !caps_lock; goto eoi; }

    /* Converte scancode para ASCII */
    if (sc < 128) {
        char c;
        bool use_shift = shift_pressed;

        /* Caps lock afeta só letras */
        if (caps_lock && sc >= 0x10 && sc <= 0x32) {
            use_shift = !use_shift;
        }

        c = use_shift ? sc_to_ascii_shift[sc] : sc_to_ascii[sc];

        if (c) {
            /* Ctrl+C, Ctrl+D, etc. */
            if (ctrl_pressed && c >= 'a' && c <= 'z') {
                c = c - 'a' + 1;  /* Ctrl+A = 1, Ctrl+C = 3, etc. */
            }
            kb_buf_push(c);
        }
    }

eoi:
    pic_send_eoi(33);
}

void keyboard_init(void) {
    kb_read_pos  = 0;
    kb_write_pos = 0;
    idt_register_handler(IRQ_KEYBOARD, keyboard_handler);
    pic_unmask_irq(1);
}

char keyboard_getchar(void) {
    if (kb_read_pos == kb_write_pos) return 0;
    char c = kb_buf[kb_read_pos];
    kb_read_pos = (kb_read_pos + 1) % KB_BUFFER_SIZE;
    return c;
}

char keyboard_read(void) {
    char c;
    while ((c = keyboard_getchar()) == 0) {
        __asm__ volatile ("hlt");
    }
    return c;
}

bool keyboard_available(void) {
    return kb_read_pos != kb_write_pos;
}
