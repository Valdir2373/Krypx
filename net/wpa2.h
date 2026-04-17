
#ifndef _NET_WPA2_H
#define _NET_WPA2_H

#include <types.h>

/* WPA2-PSK: derive 32-byte PMK from passphrase + SSID */
void wpa2_derive_pmk(const char *passphrase,
                     const char *ssid, uint8_t ssid_len,
                     uint8_t pmk[32]);

/* WPA2 PRF-384: derive PTK from PMK + addresses + nonces */
void wpa2_derive_ptk(const uint8_t pmk[32],
                     const uint8_t anonce[32], const uint8_t snonce[32],
                     const uint8_t aa[6],      const uint8_t spa[6],
                     uint8_t ptk[48]);   /* 16 KCK + 16 KEK + 16 TK */

/* Build EAPOL Key frame (msg 2 of 4-way handshake).
 * Returns frame length written into 'out'. */
uint16_t wpa2_build_eapol_msg2(const uint8_t kck[16],
                                const uint8_t anonce[32],
                                const uint8_t snonce[32],
                                const uint8_t pmkid[16],
                                uint8_t *out, uint16_t out_max);

/* Build EAPOL Key frame (msg 4 of 4-way handshake). */
uint16_t wpa2_build_eapol_msg4(const uint8_t kck[16],
                                uint64_t replay_ctr,
                                uint8_t *out, uint16_t out_max);

/* Verify MIC in received EAPOL frame using KCK.
 * 'frame' should have MIC bytes zeroed before calling (caller must restore). */
bool wpa2_verify_mic(const uint8_t kck[16],
                     const uint8_t *frame, uint16_t frame_len,
                     const uint8_t expected_mic[16]);

/* Generate random nonce */
void wpa2_gen_nonce(uint8_t nonce[32]);

/* EAPOL Key descriptor header (IEEE 802.11i) */
typedef struct __attribute__((packed)) {
    uint8_t  desc_type;      /* 2 = RSN */
    uint16_t key_info;       /* big-endian */
    uint16_t key_len;
    uint64_t replay_ctr;
    uint8_t  nonce[32];
    uint8_t  key_iv[16];
    uint8_t  rsc[8];
    uint8_t  reserved[8];
    uint8_t  mic[16];
    uint16_t key_data_len;
    /* key data follows */
} eapol_key_t;

#define EAPOL_KEY_INFO_PAIRWISE  (1<<3)
#define EAPOL_KEY_INFO_ACK       (1<<7)
#define EAPOL_KEY_INFO_MIC       (1<<8)
#define EAPOL_KEY_INFO_SECURE    (1<<9)
#define EAPOL_KEY_INFO_INSTALL   (1<<6)
#define EAPOL_KEY_INFO_ENCDATA   (1<<12)
#define EAPOL_KEY_VER_HMAC_SHA1  2

#endif
