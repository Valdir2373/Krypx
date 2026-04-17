
/* Network Manager — status de rede, reconexão DHCP, info de interface */

#include <apps/network_manager.h>
#include <gui/window.h>
#include <gui/canvas.h>
#include <proc/process.h>
#include <drivers/framebuffer.h>
#include <drivers/e1000.h>
#include <net/net.h>
#include <net/netif.h>
#include <net/dhcp.h>
#include <kernel/timer.h>
#include <lib/string.h>
#include <types.h>

static window_t *nm_win = NULL;
static char nm_status[64] = "";
static bool nm_connecting = false;
static uint32_t nm_connect_t = 0;

static void ip_to_str(uint32_t ip, char *out) {
    uint8_t *b = (uint8_t *)&ip;
    int pos = 0, i;
    for (i = 0; i < 4; i++) {
        uint8_t v = b[i];
        if (v >= 100) out[pos++] = (char)('0' + v/100);
        if (v >= 10)  out[pos++] = (char)('0' + (v/10)%10);
        out[pos++] = (char)('0' + v%10);
        if (i < 3) out[pos++] = '.';
    }
    out[pos] = '\0';
}

static void mac_to_str(const uint8_t *mac, char *out) {
    const char *hex = "0123456789ABCDEF";
    int i;
    for (i = 0; i < 6; i++) {
        out[i*3+0] = hex[(mac[i]>>4)&0xF];
        out[i*3+1] = hex[mac[i]&0xF];
        out[i*3+2] = (i < 5) ? ':' : '\0';
    }
    out[17] = '\0';
}

/* Draw a labeled value row */
static void draw_row(int lx, int *cy, const char *label, const char *value,
                     uint32_t lbl_col, uint32_t val_col, int vx) {
    canvas_set_font_scale(2);
    canvas_draw_string(lx, *cy, label, lbl_col, COLOR_TRANSPARENT);
    canvas_draw_string(vx, *cy, value, val_col, COLOR_TRANSPARENT);
    canvas_set_font_scale(2);
    *cy += 36;
}

static void nm_on_paint(window_t *win) {
    canvas_init(fb.backbuf, fb.width, fb.height, fb.pitch);
    canvas_set_font_scale(2);

    int bx = win->content_x, by = win->content_y;
    int w  = win->content_w,  h  = win->content_h;

    canvas_fill_rect(bx, by, w, h, 0x001A2332);

    /* Header */
    canvas_fill_gradient(bx, by, w, 48, 0x00003D7A, 0x001A2332);
    canvas_draw_string(bx + w/2 - 120, by + 8, "Rede", 0x0074B9FF, COLOR_TRANSPARENT);

    /* Connection status badge */
    bool up = netif_is_up();
    uint32_t badge_col = up ? 0x0000B894 : 0x00C0392B;
    canvas_fill_rounded_rect(bx + w/2 + 20, by + 10, up ? 160 : 200, 28, 6, badge_col);
    canvas_draw_string(bx + w/2 + 28, by + 16,
                       up ? "CONECTADO" : "DESCONECTADO", 0x00FFFFFF, COLOR_TRANSPARENT);

    int cy = by + 60;
    int lx = bx + 16;
    int vx = lx + 160;
    uint32_t lbl = 0x0074B9FF;
    uint32_t val = 0x00DFE6E9;

    /* Network interface rows */
    char mac_str[20];
    if (e1000_ready()) mac_to_str(net_mac, mac_str);
    else memcpy(mac_str, "N/A", 4);
    draw_row(lx, &cy, "MAC:", mac_str, lbl, val, vx);

    char ip_str[20];
    ip_to_str(net_ip ? net_ip : 0, ip_str);
    if (!net_ip) memcpy(ip_str, "0.0.0.0", 8);
    draw_row(lx, &cy, "IP:", ip_str, lbl, val, vx);

    char mask_str[20];
    if (net_mask) ip_to_str(net_mask, mask_str);
    else memcpy(mask_str, "0.0.0.0", 8);
    draw_row(lx, &cy, "Mascara:", mask_str, lbl, val, vx);

    char gw_str[20];
    if (net_gateway) ip_to_str(net_gateway, gw_str);
    else memcpy(gw_str, "0.0.0.0", 8);
    draw_row(lx, &cy, "Gateway:", gw_str, lbl, val, vx);

    char dns_str[20];
    if (net_dns) ip_to_str(net_dns, dns_str);
    else memcpy(dns_str, "0.0.0.0", 8);
    draw_row(lx, &cy, "DNS:", dns_str, lbl, val, vx);

    draw_row(lx, &cy, "Driver:",
             e1000_ready() ? "Intel e1000" : "Sem NIC", lbl, val, vx);

    /* Separator */
    cy += 4;
    canvas_draw_line(bx+8, cy, bx+w-8, cy, 0x00334455); cy += 14;

    /* DHCP reconnect button */
    int btn_w = 240, btn_h = 36;
    int btn_x = bx + (w - btn_w) / 2;
    uint32_t btn_col = nm_connecting ? 0x00555555 : 0x000984E3;
    canvas_fill_rounded_rect(btn_x, cy, btn_w, btn_h, 6, btn_col);
    const char *btn_lbl = nm_connecting ? "Conectando..." : "Reconectar (DHCP)";
    int lw = (int)strlen(btn_lbl) * 16;
    canvas_draw_string(btn_x + (btn_w - lw)/2, cy + 8, btn_lbl, 0x00FFFFFF, COLOR_TRANSPARENT);
    cy += btn_h + 12;

    /* Status message */
    if (nm_status[0]) {
        bool is_err = (nm_status[0] == 'E') || (nm_status[0] == 'T');
        canvas_draw_string(bx + (w - (int)strlen(nm_status)*16)/2, cy,
                           nm_status, is_err ? 0x00E74C3C : 0x0000B894, COLOR_TRANSPARENT);
    }

    /* Auto-update connecting status */
    if (nm_connecting && timer_get_ticks() - nm_connect_t > 4000) {
        nm_connecting = false;
        if (net_is_configured()) memcpy(nm_status, "IP obtido com sucesso!", 23);
        else memcpy(nm_status, "Timeout — sem resposta DHCP", 28);
    }
}

static void nm_on_click(window_t *win, int mx, int my) {
    int bx = win->content_x, by = win->content_y;
    int w  = win->content_w;

    /* Estimate button Y: header(48) + 6 rows(36each=216) + sep(18) = 282 */
    int btn_y = by + 282;
    int btn_w = 240, btn_h = 36;
    int btn_x = bx + (w - btn_w) / 2;

    if (!nm_connecting &&
        mx >= btn_x && mx <= btn_x + btn_w &&
        my >= btn_y && my <= btn_y + btn_h) {
        if (!e1000_ready()) {
            memcpy(nm_status, "Erro: sem placa de rede", 24);
            return;
        }
        nm_connecting = true;
        nm_connect_t  = timer_get_ticks();
        nm_status[0]  = '\0';
        dhcp_request();
    }
}

void network_manager_open(void) {
    if (nm_win && nm_win->used) { wm_focus(nm_win); return; }

    nm_win = wm_create("Gerenciador de Rede",
                        (int)fb.width/2  - 230,
                        (int)fb.height/2 - 240,
                        460, 480, WIN_RESIZABLE);
    if (!nm_win) return;
    nm_win->bg_color  = 0x001A2332;
    nm_win->on_paint  = nm_on_paint;
    nm_win->on_click  = nm_on_click;
    nm_status[0]      = '\0';
    nm_connecting     = false;
    { process_t *p = process_create_app("Rede", 48*1024);
      if (p) nm_win->proc_pid = p->pid; }
}
