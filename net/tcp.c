/*
 * net/tcp.c — Transmission Control Protocol (cliente simplificado)
 * Implementa máquina de estados TCP para conexões de saída.
 * Suporta connect, send, recv e close.
 */

#include <net/tcp.h>
#include <net/ip.h>
#include <net/net.h>
#include <net/ethernet.h>
#include <drivers/e1000.h>
#include <lib/string.h>
#include <types.h>

static tcp_conn_t conns[TCP_MAX_CONNS];
static uint16_t   next_local_port = 49152;

void tcp_init(void) {
    memset(conns, 0, sizeof(conns));
    next_local_port = 49152;
}

/* ============================================================
 * Pseudo-header para checksum TCP
 * ============================================================ */
typedef struct {
    uint32_t src;
    uint32_t dst;
    uint8_t  zero;
    uint8_t  proto;
    uint16_t tcp_len;
} __attribute__((packed)) tcp_pseudo_t;

static uint16_t tcp_checksum_calc(uint32_t src_ip, uint32_t dst_ip,
                                   const void *tcp_seg, uint16_t tcp_len)
{
    static uint8_t buf[1500];
    tcp_pseudo_t *ph = (tcp_pseudo_t *)buf;
    ph->src      = src_ip;
    ph->dst      = dst_ip;
    ph->zero     = 0;
    ph->proto    = IP_PROTO_TCP;
    ph->tcp_len  = htons(tcp_len);
    memcpy(buf + sizeof(tcp_pseudo_t), tcp_seg, tcp_len);
    return net_checksum(buf, (uint32_t)(sizeof(tcp_pseudo_t) + tcp_len));
}

/* ============================================================
 * Envia segmento TCP
 * ============================================================ */
static void tcp_send_seg(tcp_conn_t *c, uint8_t flags,
                         const void *data, uint16_t dlen)
{
    static uint8_t buf[1500];
    uint16_t total = (uint16_t)(TCP_HDR_LEN + dlen);
    tcp_hdr_t *hdr = (tcp_hdr_t *)buf;
    hdr->src_port    = htons(c->local_port);
    hdr->dst_port    = htons(c->remote_port);
    hdr->seq_num     = htonl(c->seq);
    hdr->ack_num     = (flags & TCP_ACK) ? htonl(c->ack) : 0;
    hdr->data_offset = (TCP_HDR_LEN / 4) << 4;
    hdr->flags       = flags;
    hdr->window      = htons(TCP_RECV_BUF_SZ);
    hdr->checksum    = 0;
    hdr->urgent      = 0;
    if (dlen > 0) memcpy(buf + TCP_HDR_LEN, data, dlen);
    hdr->checksum = tcp_checksum_calc(net_ip, c->remote_ip, buf, total);
    ip_send(c->remote_ip, IP_PROTO_TCP, buf, total);
}

/* ============================================================
 * Polling helper: processa um pacote da NIC
 * ============================================================ */
static void tcp_poll_once(void) {
    uint8_t tmp[2048];
    uint16_t rlen = e1000_recv(tmp, sizeof(tmp));
    if (rlen > 0) eth_recv(tmp, rlen);
}

/* ============================================================
 * API pública
 * ============================================================ */
int tcp_connect(uint32_t dst_ip, uint16_t dst_port) {
    uint32_t i;
    tcp_conn_t *c = NULL;
    for (i = 0; i < TCP_MAX_CONNS; i++) {
        if (!conns[i].used) { c = &conns[i]; break; }
    }
    if (!c) return -1;

    memset(c, 0, sizeof(*c));
    c->used        = true;
    c->remote_ip   = dst_ip;
    c->remote_port = dst_port;
    c->local_port  = next_local_port++;
    c->seq         = 0x12345678;
    c->state       = TCP_SYN_SENT;

    tcp_send_seg(c, TCP_SYN, NULL, 0);
    c->seq++;

    uint32_t timeout = 10000000;
    while (c->state != TCP_ESTABLISHED && timeout--) {
        tcp_poll_once();
        __asm__ volatile("nop");
    }

    if (c->state != TCP_ESTABLISHED) {
        c->used = false;
        return -1;
    }
    return (int)(c - conns);
}

int tcp_send(int conn_id, const void *data, uint16_t len) {
    if (conn_id < 0 || conn_id >= TCP_MAX_CONNS) return -1;
    tcp_conn_t *c = &conns[conn_id];
    if (!c->used || c->state != TCP_ESTABLISHED) return -1;

    tcp_send_seg(c, TCP_ACK | TCP_PSH, data, len);
    c->seq += len;
    return (int)len;
}

int tcp_recv(int conn_id, void *buf, uint16_t max_len) {
    if (conn_id < 0 || conn_id >= TCP_MAX_CONNS) return -1;
    tcp_conn_t *c = &conns[conn_id];
    if (!c->used) return -1;

    uint32_t timeout = 50000000;
    while (c->recv_len == 0 && timeout--) {
        tcp_poll_once();
        if (c->state == TCP_CLOSE_WAIT || c->state == TCP_CLOSED) break;
        __asm__ volatile("nop");
    }

    if (c->recv_len == 0) return 0;

    uint16_t to_copy = (c->recv_len < max_len) ? c->recv_len : max_len;
    uint32_t j;
    for (j = 0; j < to_copy; j++) {
        ((uint8_t*)buf)[j] = c->recv_buf[(c->recv_head + j) % TCP_RECV_BUF_SZ];
    }
    c->recv_head = (uint16_t)((c->recv_head + to_copy) % TCP_RECV_BUF_SZ);
    c->recv_len  = (uint16_t)(c->recv_len - to_copy);
    return (int)to_copy;
}

void tcp_close(int conn_id) {
    if (conn_id < 0 || conn_id >= TCP_MAX_CONNS) return;
    tcp_conn_t *c = &conns[conn_id];
    if (!c->used) return;

    if (c->state == TCP_ESTABLISHED || c->state == TCP_CLOSE_WAIT) {
        tcp_send_seg(c, TCP_FIN | TCP_ACK, NULL, 0);
        c->seq++;
        c->state = TCP_FIN_WAIT1;
    }
    uint32_t timeout = 5000000;
    while (c->state != TCP_CLOSED && c->state != TCP_TIME_WAIT && timeout--) {
        tcp_poll_once();
        __asm__ volatile("nop");
    }
    c->used  = false;
    c->state = TCP_CLOSED;
}

/* ============================================================
 * Recepção de segmentos TCP (chamado por ip_recv via tcp_recv_pkt)
 * ============================================================ */
void tcp_recv_pkt(const void *pkt, uint16_t len, uint32_t src_ip) {
    if (len < TCP_HDR_LEN) return;
    const tcp_hdr_t *hdr = (const tcp_hdr_t *)pkt;
    uint16_t src_port = ntohs(hdr->src_port);
    uint16_t dst_port = ntohs(hdr->dst_port);
    uint8_t  flags    = hdr->flags;
    uint32_t seq      = ntohl(hdr->seq_num);
    uint8_t  doff     = (uint8_t)((hdr->data_offset >> 4) * 4);
    const uint8_t *data = (const uint8_t *)pkt + doff;
    uint16_t dlen = (doff <= len) ? (uint16_t)(len - doff) : 0;

    uint32_t i;
    for (i = 0; i < TCP_MAX_CONNS; i++) {
        tcp_conn_t *c = &conns[i];
        if (!c->used) continue;
        if (c->remote_ip   != src_ip)   continue;
        if (c->remote_port != src_port) continue;
        if (c->local_port  != dst_port) continue;

        if (c->state == TCP_SYN_SENT) {
            if ((flags & (TCP_SYN|TCP_ACK)) == (TCP_SYN|TCP_ACK)) {
                c->ack = seq + 1;
                tcp_send_seg(c, TCP_ACK, NULL, 0);
                c->state = TCP_ESTABLISHED;
            } else if (flags & TCP_RST) {
                c->state = TCP_CLOSED;
            }
        } else if (c->state == TCP_ESTABLISHED) {
            if (flags & TCP_RST) { c->state = TCP_CLOSED; return; }
            if (dlen > 0) {
                uint16_t j;
                for (j = 0; j < dlen && c->recv_len < TCP_RECV_BUF_SZ; j++) {
                    c->recv_buf[c->recv_tail % TCP_RECV_BUF_SZ] = data[j];
                    c->recv_tail = (uint16_t)((c->recv_tail + 1) % TCP_RECV_BUF_SZ);
                    c->recv_len++;
                }
                c->ack = seq + dlen;
                tcp_send_seg(c, TCP_ACK, NULL, 0);
            }
            if (flags & TCP_FIN) {
                c->ack = seq + 1;
                tcp_send_seg(c, TCP_ACK, NULL, 0);
                c->state = TCP_CLOSE_WAIT;
            }
        } else if (c->state == TCP_FIN_WAIT1) {
            if (flags & TCP_ACK)  c->state = TCP_FIN_WAIT2;
            if (flags & TCP_FIN) {
                c->ack = seq + 1;
                tcp_send_seg(c, TCP_ACK, NULL, 0);
                c->state = TCP_TIME_WAIT;
            }
        } else if (c->state == TCP_FIN_WAIT2) {
            if (flags & TCP_FIN) {
                c->ack = seq + 1;
                tcp_send_seg(c, TCP_ACK, NULL, 0);
                c->state = TCP_TIME_WAIT;
            }
        }
        return;
    }
}
