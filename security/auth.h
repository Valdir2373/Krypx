
#ifndef _AUTH_H
#define _AUTH_H

#include <types.h>


void auth_hash_password(const char *password, uint8_t out[32]);


bool auth_check_password(const char *password, const uint8_t stored_hash[32]);

#endif 
