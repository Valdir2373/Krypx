
#ifndef _USERS_H
#define _USERS_H

#include <types.h>

#define MAX_USERS        8
#define USERNAME_MAX    32
#define HOME_DIR_MAX    64
#define PASSWORD_HASH_SZ 32   


#define PRIV_USER   0
#define PRIV_ADMIN  1
#define PRIV_ROOT   2


typedef struct {
    bool     active;
    uint32_t uid;
    uint32_t gid;
    char     username[USERNAME_MAX];
    uint8_t  password_hash[PASSWORD_HASH_SZ];
    char     home_dir[HOME_DIR_MAX];
    uint8_t  privileges;
} user_t;


extern user_t *current_user;


int user_create(const char *username, const char *password, uint8_t priv);


int user_authenticate(const char *username, const char *password);


user_t *user_find(const char *username);


user_t *user_find_by_uid(uint32_t uid);


void user_switch(uint32_t uid);


void users_init(void);

#endif 
