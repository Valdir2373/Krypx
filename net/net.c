

#include <net/net.h>
#include <types.h>


uint8_t  net_mac[6]    = {0};
uint32_t net_ip        = 0;
uint32_t net_gateway   = 0;
uint32_t net_mask      = 0;
uint32_t net_dns       = 0;

bool net_is_configured(void) {
    return net_ip != 0;
}


uint16_t net_checksum(const void *data, uint32_t len) {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    
    if (len == 1) {
        sum += *(const uint8_t *)p;
    }

    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}
