/*
 * gui/x11_server.c — Minimal X11 wire-protocol server on the Krypx framebuffer.
 *
 * Listens on Unix socket "/tmp/.X11-unix/X0" via the kernel service API.
 * Handles the core X11 requests needed for GTK3/Qt5 apps (including Firefox):
 *   Connection setup, InternAtom, CreateWindow, MapWindow, CreateGC,
 *   PolyFillRectangle, PutImage, PolyText8, QueryExtension, GetKeyboardMapping,
 *   ChangeProperty, GetProperty, SetInputFocus, QueryKeymap, GetGeometry,
 *   ConfigureWindow, ChangeWindowAttributes, GetWindowAttributes, SendEvent.
 */

#include <gui/x11_server.h>
#include <compat/linux_compat64.h>
#include <drivers/framebuffer.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <gui/canvas.h>
#include <fs/vfs.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <kernel/timer.h>
#include <types.h>

/* ── Tunables ─────────────────────────────────────────────────────────────── */
#define X11_MAX_WINDOWS   64
#define X11_MAX_GCS       64
#define X11_MAX_ATOMS    512
#define X11_RESOURCE_BASE 0x00200000UL
#define X11_RESOURCE_MASK 0x001FFFFFUL

/* ── X11 resource id allocation ───────────────────────────────────────────── */
static uint32_t g_next_xid = X11_RESOURCE_BASE | 0x10;

static uint32_t alloc_xid(void) { return g_next_xid++; }

/* ── Windows ──────────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t id;
    int32_t  x, y;
    uint32_t w, h;
    uint32_t border;
    bool     mapped;
    uint32_t bg_pixel;
    uint32_t event_mask;
    uint32_t parent;
} x11_win_t;

static x11_win_t g_wins[X11_MAX_WINDOWS];
static int       g_nwins = 0;

static x11_win_t *find_win(uint32_t id) {
    int i;
    for (i = 0; i < g_nwins; i++) if (g_wins[i].id == id) return &g_wins[i];
    return 0;
}
static x11_win_t *alloc_win(uint32_t id) {
    if (g_nwins >= X11_MAX_WINDOWS) return 0;
    memset(&g_wins[g_nwins], 0, sizeof(x11_win_t));
    g_wins[g_nwins].id = id;
    return &g_wins[g_nwins++];
}

/* ── GCs ──────────────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t id;
    uint32_t foreground, background;
    int      line_width;
    uint32_t font;
} x11_gc_t;

static x11_gc_t g_gcs[X11_MAX_GCS];
static int      g_ngcs = 0;

static x11_gc_t *find_gc(uint32_t id) {
    int i;
    for (i = 0; i < g_ngcs; i++) if (g_gcs[i].id == id) return &g_gcs[i];
    return 0;
}
static x11_gc_t *alloc_gc(uint32_t id) {
    if (g_ngcs >= X11_MAX_GCS) return 0;
    memset(&g_gcs[g_ngcs], 0, sizeof(x11_gc_t));
    g_gcs[g_ngcs].id = id;
    g_gcs[g_ngcs].foreground = 0xFFFFFFFF;
    g_gcs[g_ngcs].background = 0xFF000000;
    return &g_gcs[g_ngcs++];
}

/* ── Atoms ────────────────────────────────────────────────────────────────── */
/* Predefined X11 atoms 1-68; dynamic atoms start at 69. */
static const char *predefined_atoms[] = {
    "",              /* 0 = None */
    "PRIMARY",       /* 1 */
    "SECONDARY",     /* 2 */
    "ARC",           /* 3 */
    "ATOM",          /* 4 */
    "BITMAP",        /* 5 */
    "CARDINAL",      /* 6 */
    "COLORMAP",      /* 7 */
    "CURSOR",        /* 8 */
    "CUT_BUFFER0",   /* 9 */
    "CUT_BUFFER1",   /* 10 */
    "CUT_BUFFER2",   /* 11 */
    "CUT_BUFFER3",   /* 12 */
    "CUT_BUFFER4",   /* 13 */
    "CUT_BUFFER5",   /* 14 */
    "CUT_BUFFER6",   /* 15 */
    "CUT_BUFFER7",   /* 16 */
    "DRAWABLE",      /* 17 */
    "FONT",          /* 18 */
    "INTEGER",       /* 19 */
    "PIXMAP",        /* 20 */
    "POINT",         /* 21 */
    "RECTANGLE",     /* 22 */
    "RESOURCE_MANAGER",/* 23 */
    "RGB_COLOR_MAP", /* 24 */
    "RGB_BEST_MAP",  /* 25 */
    "RGB_BLUE_MAP",  /* 26 */
    "RGB_DEFAULT_MAP",/* 27 */
    "RGB_GRAY_MAP",  /* 28 */
    "RGB_GREEN_MAP", /* 29 */
    "RGB_RED_MAP",   /* 30 */
    "STRING",        /* 31 */
    "VISUALID",      /* 32 */
    "WINDOW",        /* 33 */
    "WM_COMMAND",    /* 34 */
    "WM_HINTS",      /* 35 */
    "WM_CLIENT_MACHINE",/* 36 */
    "WM_ICON_NAME",  /* 37 */
    "WM_ICON_SIZE",  /* 38 */
    "WM_NAME",       /* 39 */
    "WM_NORMAL_HINTS",/* 40 */
    "WM_SIZE_HINTS", /* 41 */
    "WM_ZOOM_HINTS", /* 42 */
    "MIN_SPACE",     /* 43 */
    "NORM_SPACE",    /* 44 */
    "MAX_SPACE",     /* 45 */
    "END_SPACE",     /* 46 */
    "SUPERSCRIPT_X", /* 47 */
    "SUPERSCRIPT_Y", /* 48 */
    "SUBSCRIPT_X",   /* 49 */
    "SUBSCRIPT_Y",   /* 50 */
    "UNDERLINE_POSITION",/* 51 */
    "UNDERLINE_THICKNESS",/* 52 */
    "STRIKEOUT_ASCENT",/* 53 */
    "STRIKEOUT_DESCENT",/* 54 */
    "ITALIC_ANGLE",  /* 55 */
    "X_HEIGHT",      /* 56 */
    "QUAD_WIDTH",    /* 57 */
    "WEIGHT",        /* 58 */
    "POINT_SIZE",    /* 59 */
    "RESOLUTION",    /* 60 */
    "COPYRIGHT",     /* 61 */
    "NOTICE",        /* 62 */
    "FONT_NAME",     /* 63 */
    "FAMILY_NAME",   /* 64 */
    "FULL_NAME",     /* 65 */
    "CAP_HEIGHT",    /* 66 */
    "WM_CLASS",      /* 67 */
    "WM_TRANSIENT_FOR",/* 68 */
};
#define N_PREDEF_ATOMS 69

static char   g_atom_names[X11_MAX_ATOMS][64];
static int    g_natoms = N_PREDEF_ATOMS;

static uint32_t intern_atom(const char *name, int only_if_exists) {
    /* Search predefined */
    uint32_t i;
    for (i = 1; i < N_PREDEF_ATOMS; i++)
        if (strcmp(name, predefined_atoms[i]) == 0) return i;
    /* Search dynamic */
    for (i = N_PREDEF_ATOMS; i < (uint32_t)g_natoms; i++)
        if (strcmp(name, g_atom_names[i]) == 0) return i;
    if (only_if_exists) return 0;
    if (g_natoms >= X11_MAX_ATOMS) return 0;
    strncpy(g_atom_names[g_natoms], name, 63);
    return (uint32_t)(g_natoms++);
}

static const char *atom_name(uint32_t atom) {
    if (atom == 0) return "None";
    if (atom < N_PREDEF_ATOMS) return predefined_atoms[atom];
    if (atom < (uint32_t)g_natoms) return g_atom_names[atom];
    return "";
}

/* ── Wire helpers ─────────────────────────────────────────────────────────── */
static int  g_svc = -1;     /* kernel service index */
static uint32_t g_seq = 0;  /* sequence counter */

/* Append bytes to output buffer */
static uint8_t  g_outbuf[65536];
static uint32_t g_outlen = 0;

static void out_u8(uint8_t v)  { if (g_outlen<65535) g_outbuf[g_outlen++]=v; }
static void out_u16(uint16_t v){ out_u8((uint8_t)(v&0xFF)); out_u8((uint8_t)(v>>8)); }
static void out_u32(uint32_t v){ out_u16((uint16_t)(v&0xFFFF)); out_u16((uint16_t)(v>>16)); }
static void out_zero(uint32_t n){ uint32_t i; for(i=0;i<n;i++) out_u8(0); }
static void out_pad4(uint32_t len) {
    uint32_t p = (4 - (len & 3)) & 3;
    out_zero(p);
}
static void out_str(const char *s, uint16_t len) {
    uint16_t i;
    for (i = 0; i < len; i++) out_u8((uint8_t)s[i]);
}

static void flush_out(void) {
    if (g_outlen == 0 || g_svc < 0) return;
    lx64_ksvc_write(g_svc, g_outbuf, g_outlen);
    g_outlen = 0;
}

/* ── Wire input ───────────────────────────────────────────────────────────── */
static uint8_t  g_inbuf[65536];
static uint32_t g_inlen = 0;

static uint8_t  in_u8(void)  { return (g_inlen > 0) ? g_inbuf[0] : 0; }
static uint16_t in_u16_at(const uint8_t *b) { return (uint16_t)(b[0] | (b[1]<<8)); }
static uint32_t in_u32_at(const uint8_t *b) {
    return (uint32_t)(b[0]|(b[1]<<8)|(b[2]<<16)|(b[3]<<24));
}

/* ── Root / visual IDs ────────────────────────────────────────────────────── */
#define ROOT_WIN_ID   (X11_RESOURCE_BASE | 1)
#define ROOT_CMAP_ID  (X11_RESOURCE_BASE | 2)
#define ROOT_VISUAL   (X11_RESOURCE_BASE | 3)

/* ── Connection setup response ────────────────────────────────────────────── */
static void send_connection_accepted(void) {
    extern framebuffer_t fb;
    uint16_t fw = fb.width  ? (uint16_t)fb.width  : 1024;
    uint16_t fh = fb.height ? (uint16_t)fb.height : 768;

    const char *vendor = "Krypx X Server";
    uint16_t vlen = (uint16_t)strlen(vendor);
    uint16_t vpad = (uint16_t)((4 - (vlen & 3)) & 3);

    /* Fixed fields after 8-byte header:
     *   release(4) + resource_base(4) + resource_mask(4) + motion_buf(4)
     *   vendor_len(2) + max_req_len(2) + num_roots(1) + num_formats(1)
     *   image_order(1) + bitmap_order(1) + bitmap_unit(1) + bitmap_pad(1)
     *   min_keycode(1) + max_keycode(1) + pad(4)
     *   = 32 bytes
     * + vendor (padded)
     * + 1 format × 8 bytes
     * + 1 screen:
     *     root..input_masks: 20 bytes
     *     widths/heights: 8 bytes, min/max maps: 4 bytes
     *     root_visual(4) + backing(1)+save_unders(1)+root_depth(1)+ndepths(1)
     *     = 36 bytes + 1 depth (8) + 1 visual (24) = 68 bytes
     */
    uint32_t extra = 32 + vlen + vpad + 8 + 68;
    uint16_t extra_words = (uint16_t)(extra / 4);

    /* 8-byte header */
    out_u8(1);               /* success */
    out_u8(0);               /* pad */
    out_u16(11);             /* major */
    out_u16(0);              /* minor */
    out_u16(extra_words);

    /* release, resource base/mask, motion buffer size */
    out_u32(1);
    out_u32(X11_RESOURCE_BASE);
    out_u32(X11_RESOURCE_MASK);
    out_u32(256);

    /* vendor length, max-request-length */
    out_u16(vlen);
    out_u16(65535);

    /* num_roots, num_formats, byte orders, bitmap geometry, keycodes, pad */
    out_u8(1); out_u8(1);
    out_u8(0); out_u8(0);     /* LSBFirst */
    out_u8(32); out_u8(32);   /* bitmap unit/pad */
    out_u8(8); out_u8(255);   /* min/max keycode */
    out_u32(0);               /* pad */

    /* vendor string */
    out_str(vendor, vlen);
    out_zero(vpad);

    /* Pixmap format: depth=24, bpp=32, scanline_pad=32, pad×5 */
    out_u8(24); out_u8(32); out_u8(32); out_zero(5);

    /* Screen */
    out_u32(ROOT_WIN_ID);
    out_u32(ROOT_CMAP_ID);
    out_u32(0x00FFFFFF);   /* white */
    out_u32(0x00000000);   /* black */
    out_u32(0);            /* input-masks */
    out_u16(fw); out_u16(fh);
    out_u16(305); out_u16(178); /* mm */
    out_u16(1); out_u16(1);     /* min/max installed maps */
    out_u32(ROOT_VISUAL);
    out_u8(0); out_u8(0); out_u8(24); out_u8(1); /* backing/save/depth/ndepths */

    /* Depth entry: depth=24, pad, nvisuals=1, pad */
    out_u8(24); out_u8(0); out_u16(1); out_u32(0);

    /* Visual entry */
    out_u32(ROOT_VISUAL);
    out_u8(4);    /* TrueColor */
    out_u8(8);    /* bits-per-rgb */
    out_u16(256); /* colormap-entries */
    out_u32(0x00FF0000); /* red */
    out_u32(0x0000FF00); /* green */
    out_u32(0x000000FF); /* blue */
    out_u32(0);

    flush_out();
}

/* ── Reply builders ───────────────────────────────────────────────────────── */
static void send_error(uint8_t err_code, uint32_t bad_val, uint16_t minor_op, uint8_t major_op) {
    out_u8(0);         /* Error */
    out_u8(err_code);
    out_u16((uint16_t)g_seq);
    out_u32(bad_val);
    out_u16(minor_op);
    out_u8(major_op);
    out_zero(21);
    flush_out();
}

/* Generic 32-byte reply with 24 bytes of payload (extra_len=0) */
static void reply_start(uint8_t extra) {
    out_u8(1);          /* reply */
    out_u8(extra);
    out_u16((uint16_t)g_seq);
}

/* ── Request handlers ─────────────────────────────────────────────────────── */

/* opcode 16: InternAtom */
static void handle_intern_atom(const uint8_t *req, uint32_t /*len*/) {
    uint8_t only_if_exists = req[1];
    uint16_t name_len = in_u16_at(req + 4);
    const char *name = (const char*)(req + 8);
    char tmp[128];
    uint32_t n = name_len < 127 ? name_len : 127;
    memcpy(tmp, name, n); tmp[n] = '\0';
    uint32_t atom = intern_atom(tmp, only_if_exists);

    reply_start(0);
    out_u32(0);    /* extra data length = 0 */
    out_u32(atom);
    out_zero(20);
    flush_out();
}

/* opcode 17: GetAtomName */
static void handle_get_atom_name(const uint8_t *req) {
    uint32_t atom = in_u32_at(req + 4);
    const char *name = atom_name(atom);
    uint16_t nlen = (uint16_t)strlen(name);
    uint16_t npad = (uint16_t)((4 - (nlen & 3)) & 3);

    reply_start(0);
    out_u32((uint32_t)(nlen + npad) / 4);
    out_u16(nlen);
    out_zero(22);
    out_str(name, nlen);
    out_zero(npad);
    flush_out();
}

/* opcode 1: CreateWindow */
static void handle_create_window(const uint8_t *req, uint32_t len) {
    uint32_t wid    = in_u32_at(req + 4);
    uint32_t parent = in_u32_at(req + 8);
    int32_t  x      = (int32_t)in_u16_at(req + 12);
    int32_t  y      = (int32_t)in_u16_at(req + 14);
    uint16_t w      = in_u16_at(req + 16);
    uint16_t h      = in_u16_at(req + 18);
    (void)len;
    x11_win_t *win = alloc_win(wid);
    if (!win) { send_error(5 /*Alloc*/, wid, 0, 1); return; }
    win->x = x; win->y = y;
    win->w = w; win->h = h;
    win->parent = parent;
    win->bg_pixel = 0xFF2D3436;
    /* no reply for CreateWindow */
}

/* opcode 2: ChangeWindowAttributes */
static void handle_change_window_attrs(const uint8_t *req) {
    uint32_t wid  = in_u32_at(req + 4);
    uint32_t mask = in_u32_at(req + 8);
    const uint8_t *vp = req + 12;
    x11_win_t *win = find_win(wid);
    if (!win) return;
    /* bit 1 = CWBackPixel, bit 11 = CWEventMask */
    uint32_t bit; const uint8_t *p = vp;
    for (bit = 0; bit < 32; bit++) {
        if (!(mask & (1u << bit))) continue;
        uint32_t v = in_u32_at(p); p += 4;
        if (bit == 1)  win->bg_pixel   = v;
        if (bit == 11) win->event_mask = v;
    }
}

/* opcode 3: GetWindowAttributes */
static void handle_get_window_attrs(const uint8_t *req) {
    uint32_t wid = in_u32_at(req + 4);
    x11_win_t *win = find_win(wid);
    if (!win) { send_error(3 /*Window*/, wid, 0, 3); return; }

    reply_start(win->mapped ? 1 : 0);
    out_u32(3);               /* extra data = 3 × 4 = 12 bytes */
    out_u32(ROOT_VISUAL);
    out_u16(1);               /* class = InputOutput */
    out_u8(0); out_u8(0);     /* bit-gravity, win-gravity */
    out_u32(win->event_mask);
    out_u32(win->event_mask); /* do-not-propagate */
    out_u8(0);                /* override-redirect */
    out_u8(win->mapped ? 2 : 0); /* map-state: 0=Unmapped, 2=Viewable */
    out_zero(10);
    out_u32(ROOT_CMAP_ID);
    out_u32(1);               /* all-event-masks */
    flush_out();
}

/* opcode 8: MapWindow */
static void handle_map_window(const uint8_t *req) {
    uint32_t wid = in_u32_at(req + 4);
    x11_win_t *win = find_win(wid);
    if (!win) return;
    win->mapped = true;
    /* Send Expose event */
    out_u8(12);              /* Expose event */
    out_u8(0);
    out_u16((uint16_t)g_seq);
    out_u32(wid);
    out_u16((uint16_t)win->x); out_u16((uint16_t)win->y);
    out_u16((uint16_t)win->w); out_u16((uint16_t)win->h);
    out_u16(0);              /* count = 0 (last expose) */
    out_zero(14);
    flush_out();
}

/* opcode 10: UnmapWindow */
static void handle_unmap_window(const uint8_t *req) {
    uint32_t wid = in_u32_at(req + 4);
    x11_win_t *win = find_win(wid);
    if (win) win->mapped = false;
}

/* opcode 14: GetGeometry */
static void handle_get_geometry(const uint8_t *req) {
    uint32_t drawable = in_u32_at(req + 4);
    x11_win_t *win = find_win(drawable);
    extern framebuffer_t fb;
    uint16_t w = win ? (uint16_t)win->w : (uint16_t)(fb.width  ? fb.width  : 1024);
    uint16_t h = win ? (uint16_t)win->h : (uint16_t)(fb.height ? fb.height : 768);

    reply_start(24); /* depth=24 */
    out_u32(0);
    out_u32(ROOT_WIN_ID);
    out_u16(win ? (uint16_t)win->x : 0);
    out_u16(win ? (uint16_t)win->y : 0);
    out_u16(w); out_u16(h);
    out_u16(0); /* border */
    out_zero(10);
    flush_out();
}

/* opcode 12: ConfigureWindow */
static void handle_configure_window(const uint8_t *req) {
    uint32_t wid  = in_u32_at(req + 4);
    uint16_t mask = in_u16_at(req + 8);
    const uint8_t *vp = req + 12;
    x11_win_t *win = find_win(wid);
    uint16_t bit;
    const uint8_t *p = vp;
    for (bit = 0; bit < 7; bit++) {
        if (!(mask & (1u << bit))) continue;
        uint32_t v = in_u32_at(p); p += 4;
        if (!win) continue;
        if (bit == 0) win->x = (int32_t)v;
        if (bit == 1) win->y = (int32_t)v;
        if (bit == 2) win->w = v;
        if (bit == 3) win->h = v;
    }
}

/* opcode 50: CreateGC */
static void handle_create_gc(const uint8_t *req) {
    uint32_t gc_id = in_u32_at(req + 4);
    uint32_t mask  = in_u32_at(req + 12);
    const uint8_t *vp = req + 16;
    x11_gc_t *gc = alloc_gc(gc_id);
    if (!gc) return;
    const uint8_t *p = vp;
    uint32_t bit;
    for (bit = 0; bit < 32; bit++) {
        if (!(mask & (1u << bit))) continue;
        uint32_t v = in_u32_at(p); p += 4;
        if (bit == 2)  gc->foreground = v;
        if (bit == 3)  gc->background = v;
        if (bit == 4)  gc->line_width = (int)v;
        if (bit == 14) gc->font = v;
    }
}

/* opcode 51: ChangeGC */
static void handle_change_gc(const uint8_t *req) {
    uint32_t gc_id = in_u32_at(req + 4);
    uint32_t mask  = in_u32_at(req + 8);
    const uint8_t *vp = req + 12;
    x11_gc_t *gc = find_gc(gc_id);
    if (!gc) return;
    const uint8_t *p = vp;
    uint32_t bit;
    for (bit = 0; bit < 32; bit++) {
        if (!(mask & (1u << bit))) continue;
        uint32_t v = in_u32_at(p); p += 4;
        if (bit == 2)  gc->foreground = v;
        if (bit == 3)  gc->background = v;
        if (bit == 14) gc->font = v;
    }
}

/* opcode 53: FreeGC — no-op */
static void handle_free_gc(const uint8_t *req) {
    uint32_t gc_id = in_u32_at(req + 4);
    int i;
    for (i = 0; i < g_ngcs; i++) {
        if (g_gcs[i].id == gc_id) {
            g_gcs[i] = g_gcs[--g_ngcs];
            break;
        }
    }
}

/* opcode 66: PolyFillRectangle */
static void handle_fill_rect(const uint8_t *req, uint32_t len) {
    uint32_t drawable = in_u32_at(req + 4);
    uint32_t gc_id    = in_u32_at(req + 8);
    x11_gc_t *gc = find_gc(gc_id);
    uint32_t color = gc ? gc->foreground : 0xFFFFFFFF;
    (void)drawable;
    const uint8_t *rp = req + 12;
    uint32_t n = (len - 12) / 8;
    uint32_t i;
    for (i = 0; i < n; i++) {
        int16_t rx = (int16_t)in_u16_at(rp);
        int16_t ry = (int16_t)in_u16_at(rp+2);
        uint16_t rw = in_u16_at(rp+4);
        uint16_t rh = in_u16_at(rp+6);
        rp += 8;
        fb_fill_rect((int)rx, (int)ry, (int)rw, (int)rh, color);
    }
}

/* opcode 72: PutImage (ZPixmap only) */
static void handle_put_image(const uint8_t *req) {
    uint8_t  format   = req[1];     /* 0=Bitmap,1=XYPixmap,2=ZPixmap */
    uint32_t drawable = in_u32_at(req + 4);
    uint32_t gc_id    = in_u32_at(req + 8);
    uint16_t width    = in_u16_at(req + 12);
    uint16_t height   = in_u16_at(req + 14);
    int16_t  dst_x    = (int16_t)in_u16_at(req + 16);
    int16_t  dst_y    = (int16_t)in_u16_at(req + 18);
    (void)drawable; (void)gc_id;
    if (format != 2 || width == 0 || height == 0) return;
    const uint32_t *pixels = (const uint32_t*)(req + 24);
    fb_blit((int)dst_x, (int)dst_y, (int)width, (int)height, (uint32_t*)pixels);
}

/* opcode 74: PolyText8 / opcode 76: ImageText8 */
static void handle_text8(const uint8_t *req) {
    uint32_t drawable = in_u32_at(req + 4);
    uint32_t gc_id    = in_u32_at(req + 8);
    int16_t  x        = (int16_t)in_u16_at(req + 12);
    int16_t  y        = (int16_t)in_u16_at(req + 14);
    x11_gc_t *gc = find_gc(gc_id);
    uint32_t fg = gc ? gc->foreground : 0xFFFFFFFF;
    uint32_t bg = gc ? gc->background : 0xFF000000;
    (void)drawable;
    /* ImageText8: opcode 76 has string right at offset 16 */
    uint8_t slen = req[4];   /* for ImageText8 this is in req[1] */
    /* PolyText8: skip items */
    const char *s = (const char*)(req + 16);
    if (req[0] == 76) { /* ImageText8 */
        slen = req[1];
        s    = (const char*)(req + 16);
    } else {
        /* PolyText8: first item header */
        uint8_t icount = req[16];
        if (icount == 0) return;
        s    = (const char*)(req + 18);
        slen = icount;
    }
    canvas_draw_string((int)x, (int)y - 12, s, fg, bg);
}

/* opcode 84: AllocColor */
static void handle_alloc_color(const uint8_t *req) {
    uint16_t r = in_u16_at(req + 8);
    uint16_t g = in_u16_at(req + 10);
    uint16_t b = in_u16_at(req + 12);
    uint32_t pixel = ((uint32_t)(r>>8) << 16) | ((uint32_t)(g>>8) << 8) | (uint32_t)(b>>8);

    reply_start(0);
    out_u32(0);
    out_u16(r); out_u16(g); out_u16(b);
    out_u16(0);
    out_u32(pixel);
    out_zero(12);
    flush_out();
}

/* opcode 98: QueryExtension */
static void handle_query_extension(const uint8_t *req) {
    uint16_t name_len = in_u16_at(req + 4);
    const char *name  = (const char*)(req + 8);
    char tmp[64];
    uint32_t n = name_len < 63 ? name_len : 63;
    memcpy(tmp, name, n); tmp[n] = '\0';

    /* Extensions we support (stub) */
    uint8_t present = 0, major_op = 0, first_event = 0, first_error = 0;
    if (strcmp(tmp, "BIG-REQUESTS") == 0)    { present=1; major_op=133; }
    if (strcmp(tmp, "MIT-SHM") == 0)         { present=1; major_op=130; first_event=65; }
    if (strcmp(tmp, "RENDER") == 0)          { present=1; major_op=139; }
    if (strcmp(tmp, "Composite") == 0)       { present=1; major_op=142; }
    if (strcmp(tmp, "XFIXES") == 0)          { present=1; major_op=138; }
    if (strcmp(tmp, "DAMAGE") == 0)          { present=1; major_op=143; first_event=91; }
    if (strcmp(tmp, "RANDR") == 0)           { present=1; major_op=140; first_event=89; }
    if (strcmp(tmp, "XINERAMA") == 0)        { present=1; major_op=141; }
    if (strcmp(tmp, "XInputExtension") == 0) { present=1; major_op=131; first_event=80; }
    if (strcmp(tmp, "SYNC") == 0)            { present=1; major_op=134; first_event=83; }
    if (strcmp(tmp, "SHAPE") == 0)           { present=1; major_op=129; first_event=64; }
    if (strcmp(tmp, "DOUBLE-BUFFER") == 0)   { present=1; major_op=135; }

    reply_start(0);
    out_u32(0);
    out_u8(present); out_u8(major_op); out_u8(first_event); out_u8(first_error);
    out_zero(20);
    flush_out();
}

/* opcode 20: GetProperty */
static void handle_get_property(const uint8_t *req) {
    /* Return empty property for everything — clients usually handle gracefully */
    reply_start(0);  /* format = 0 */
    out_u32(0);      /* no extra data */
    out_u32(0);      /* type = None */
    out_u32(0);      /* bytes-after = 0 */
    out_u32(0);      /* value-len = 0 */
    out_zero(12);
    flush_out();
}

/* opcode 101: GetKeyboardMapping */
static void handle_get_keyboard_mapping(const uint8_t *req) {
    uint8_t first = req[4];
    uint8_t count = req[5];
    uint8_t keysyms_per = 2;

    reply_start(keysyms_per);
    out_u32((uint32_t)(count * keysyms_per));  /* extra data = count*kpk words */
    out_zero(24);

    /* Emit keysym pairs (keysym, shift_keysym) for each keycode */
    uint8_t i;
    for (i = 0; i < count; i++) {
        uint8_t kc = first + i;
        /* Simple mapping: keycodes 10-19 → digit keys 1-9,0 */
        uint32_t ks = 0;
        if (kc >= 10 && kc <= 18) ks = 0x0031 + (kc - 10);  /* '1'..'9' */
        if (kc == 19) ks = 0x0030;                             /* '0' */
        if (kc >= 24 && kc <= 32) ks = 0x0071 + (kc - 24);  /* 'q'..'i' */
        if (kc >= 38 && kc <= 46) ks = 0x0061 + (kc - 38);  /* 'a'..'k' */
        if (kc == 65) ks = 0x0020;                             /* space */
        if (kc == 36) ks = 0xFF0D;                             /* Return */
        if (kc == 9)  ks = 0xFF1B;                             /* Escape */
        out_u32(ks);
        out_u32(ks ? ks - 0x20 : 0);  /* shift variant (uppercase) */
    }
    flush_out();
}

/* opcode 43: QueryKeymap */
static void handle_query_keymap(void) {
    reply_start(0);
    out_u32(2);    /* extra = 2 words = 8 bytes */
    out_zero(24);
    out_zero(32);  /* 32 bytes keymap = all keys up */
    flush_out();
}

/* opcode 40: GetInputFocus */
static void handle_get_input_focus(void) {
    reply_start(1); /* revert-to: 1=PointerRoot */
    out_u32(0);
    out_u32(ROOT_WIN_ID); /* focus = root */
    out_zero(20);
    flush_out();
}

/* opcode 133 (BIG-REQUESTS): BigReqEnable */
static void handle_big_requests(const uint8_t *req) {
    (void)req;
    reply_start(0);
    out_u32(0);
    out_u32(0x40000000UL);  /* max request size in 4-byte units */
    out_zero(20);
    flush_out();
}

/* Extension stubs that just send a reply */
static void handle_extension_stub(void) {
    reply_start(0);
    out_u32(0); out_zero(24);
    flush_out();
}

/* ── X11 server event injection ───────────────────────────────────────────── */
static uint32_t g_focus_win = ROOT_WIN_ID;

void x11_inject_key(uint8_t keycode, bool pressed, uint32_t state) {
    if (g_svc < 0 || !lx64_ksvc_has_client(g_svc)) return;
    /* Find a window that has KeyPress/Release in its event_mask */
    uint32_t target = g_focus_win;
    out_u8(pressed ? 2 : 3); /* KeyPress=2, KeyRelease=3 */
    out_u8(keycode);
    out_u16((uint16_t)++g_seq);
    out_u32(timer_get_ticks()); /* time */
    out_u32(ROOT_WIN_ID);        /* root */
    out_u32(target);             /* event */
    out_u32(target);             /* child */
    out_u16(0); out_u16(0);      /* root-x, root-y */
    out_u16(0); out_u16(0);      /* event-x, event-y */
    out_u16((uint16_t)state);    /* state */
    out_u8(1);                   /* same-screen */
    out_u8(0);
    flush_out();
}

void x11_inject_mouse(int abs_x, int abs_y, int dx, int dy, uint8_t buttons) {
    (void)dx; (void)dy;
    if (g_svc < 0 || !lx64_ksvc_has_client(g_svc)) return;
    out_u8(6); /* MotionNotify */
    out_u8(0); /* detail */
    out_u16((uint16_t)++g_seq);
    out_u32(timer_get_ticks());
    out_u32(ROOT_WIN_ID);
    out_u32(ROOT_WIN_ID);
    out_u32(0);  /* child */
    out_u16((uint16_t)abs_x); out_u16((uint16_t)abs_y);
    out_u16((uint16_t)abs_x); out_u16((uint16_t)abs_y);
    out_u16(buttons); /* state */
    out_u8(1); out_u8(0);
    flush_out();
}

/* ── Main dispatch ────────────────────────────────────────────────────────── */
static bool g_client_connected = false;

/* Process one X11 request from inbuf.  Returns bytes consumed, or 0 if incomplete. */
static uint32_t dispatch_request(const uint8_t *req, uint32_t avail) {
    if (avail < 4) return 0;
    uint8_t opcode = req[0];
    uint32_t req_len = (uint32_t)in_u16_at(req + 2) * 4;
    if (req_len == 0) req_len = 4;
    if (avail < req_len) return 0;

    g_seq++;

    switch (opcode) {
    case   1: handle_create_window(req, req_len); break;
    case   2: handle_change_window_attrs(req); break;
    case   3: handle_get_window_attrs(req); break;
    case   4: /* DestroyWindow */ break;
    case   8: handle_map_window(req); break;
    case  10: handle_unmap_window(req); break;
    case  12: handle_configure_window(req); break;
    case  14: handle_get_geometry(req); break;
    case  16: handle_intern_atom(req, req_len); break;
    case  17: handle_get_atom_name(req); break;
    case  18: /* ChangeProperty — no reply */ break;
    case  20: handle_get_property(req); break;
    case  21: /* ListProperties */ handle_extension_stub(); break;
    case  25: /* SendEvent */ break;
    case  26: /* GrabPointer */ handle_extension_stub(); break;
    case  28: /* UngrabPointer */ break;
    case  31: /* GrabKeyboard */ handle_extension_stub(); break;
    case  33: /* UngrabKeyboard */ break;
    case  39: /* SetInputFocus */
        g_focus_win = in_u32_at(req + 4);
        break;
    case  40: handle_get_input_focus(); break;
    case  43: handle_query_keymap(); break;
    case  45: /* OpenFont */ break;
    case  47: /* QueryFont */ handle_extension_stub(); break;
    case  49: /* ListFonts */ handle_extension_stub(); break;
    case  50: handle_create_gc(req); break;
    case  51: handle_change_gc(req); break;
    case  53: handle_free_gc(req); break;
    case  55: /* ClearArea */ {
        uint32_t wid = in_u32_at(req+4);
        x11_win_t *w = find_win(wid);
        if (w) fb_fill_rect(w->x, w->y, (int)w->w, (int)w->h, w->bg_pixel);
        break;
    }
    case  60: /* FreePixmap */ break;
    case  65: /* FillPoly */ break;
    case  66: handle_fill_rect(req, req_len); break;
    case  72: handle_put_image(req); break;
    case  74: /* fallthrough */
    case  76: handle_text8(req); break;
    case  79: /* CreateColormap */ break;
    case  81: /* FreeColormap */ break;
    case  84: handle_alloc_color(req); break;
    case  85: /* AllocNamedColor */ handle_extension_stub(); break;
    case  91: /* QueryColors */ handle_extension_stub(); break;
    case  98: handle_query_extension(req); break;
    case  99: /* ListExtensions */ handle_extension_stub(); break;
    case 101: handle_get_keyboard_mapping(req); break;
    case 102: /* ChangeKeyboardMapping */ break;
    case 103: /* GetKeyboardControl */ handle_extension_stub(); break;
    case 107: /* SetModifierMapping */ handle_extension_stub(); break;
    case 115: /* GetMotionEvents */ handle_extension_stub(); break;
    case 116: /* TranslateCoords */ handle_extension_stub(); break;
    case 117: /* WarpPointer */ break;
    case 118: /* SetInputFocus (alias) */ break;
    case 119: /* GetInputFocus (alias) */ handle_get_input_focus(); break;

    /* BIG-REQUESTS extension (major opcode 133) */
    case 133: handle_big_requests(req); break;

    /* RENDER extension stub */
    case 139: handle_extension_stub(); break;

    default:
        /* Unknown request — send BadRequest error for requests that expect replies */
        break;
    }
    return req_len;
}

/* ── x11_server_process — called from sock_read_fn when client reads ──────── */
void x11_server_process(int svc_idx) {
    if (svc_idx != g_svc || g_svc < 0) return;

    /* Pull any new data into g_inbuf */
    uint8_t tmp[4096];
    int nr;
    while ((nr = lx64_ksvc_read(g_svc, tmp, sizeof(tmp))) > 0) {
        uint32_t copy = (uint32_t)nr;
        if (g_inlen + copy > sizeof(g_inbuf)) copy = (uint32_t)sizeof(g_inbuf) - g_inlen;
        memcpy(g_inbuf + g_inlen, tmp, copy);
        g_inlen += copy;
    }

    if (!g_client_connected) {
        /* First byte is endian byte ('l'=0x6C LE, 'B'=0x42 BE) */
        if (g_inlen < 12) return;
        /* Skip connection setup request — consume it and send acceptance */
        uint16_t auth_name_len = in_u16_at(g_inbuf + 6);
        uint16_t auth_data_len = in_u16_at(g_inbuf + 8);
        uint32_t setup_len = 12
            + ((auth_name_len + 3) & ~3u)
            + ((auth_data_len + 3) & ~3u);
        if (g_inlen < setup_len) return;
        /* Consume connection request */
        g_inlen -= setup_len;
        memmove(g_inbuf, g_inbuf + setup_len, g_inlen);
        g_client_connected = true;
        send_connection_accepted();
        return;
    }

    /* Dispatch pending requests */
    uint32_t pos = 0;
    while (pos < g_inlen) {
        uint32_t consumed = dispatch_request(g_inbuf + pos, g_inlen - pos);
        if (!consumed) break;
        pos += consumed;
    }
    if (pos > 0) {
        g_inlen -= pos;
        if (g_inlen) memmove(g_inbuf, g_inbuf + pos, g_inlen);
    }
}

/* ── Public API ───────────────────────────────────────────────────────────── */
void x11_server_init(void) {
    /* Reset state */
    g_nwins = 0; g_ngcs = 0;
    g_natoms = N_PREDEF_ATOMS;
    g_seq = 0; g_outlen = 0; g_inlen = 0;
    g_client_connected = false;
    g_focus_win = ROOT_WIN_ID;

    /* Create the root window */
    extern framebuffer_t fb;
    x11_win_t *root = alloc_win(ROOT_WIN_ID);
    if (root) {
        root->w = fb.width  ? fb.width  : 1024;
        root->h = fb.height ? fb.height : 768;
        root->mapped = true;
        root->bg_pixel = 0xFF0C2461;
    }

    /* Register Unix socket so Linux processes can connect */
    g_svc = lx64_register_kernel_service(X11_SOCKET_PATH);
    if (g_svc < 0) return;

    /* Also register abstract namespace path */
    lx64_register_kernel_service("\0" X11_SOCKET_PATH);

    /* Create /tmp/.X11-unix/ directory in VFS if needed */
    vfs_node_t *tmp_dir = vfs_resolve("/tmp");
    if (tmp_dir) {
        extern int vfs_mkdir(vfs_node_t *, const char *, uint32_t);
        vfs_mkdir(tmp_dir, ".X11-unix", 0777);
    }
}

void x11_server_poll(void) {
    /* Called periodically — process any pending client data */
    if (g_svc >= 0 && lx64_ksvc_has_client(g_svc))
        x11_server_process(g_svc);
}
