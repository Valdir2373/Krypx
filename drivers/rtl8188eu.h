
#ifndef _DRIVERS_RTL8188EU_H
#define _DRIVERS_RTL8188EU_H

#include <types.h>

/* Detect and initialise the RTL8188EU USB WiFi adapter.
 * Loads firmware from /rtl8188eu.bin on the FAT32 disk.
 * Returns true if the adapter was found and initialised. */
bool rtl8188eu_init(void);

/* True if RTL8188EU was detected */
bool rtl8188eu_present(void);

/* Read MAC address into buf[6] */
void rtl8188eu_get_mac(uint8_t buf[6]);

/* Start passive+active scan on all channels */
void rtl8188eu_scan(void);

/* Send an 802.11 frame (called by wifi.c) */
void rtl8188eu_send_frame(const uint8_t *frame, uint16_t len);

/* Set the channel (1–14) */
void rtl8188eu_set_channel(uint8_t ch);

/* Poll: check for received frames, call wifi_rx_frame() for each */
void rtl8188eu_poll(void);

#endif
