
#ifndef _PERMISSIONS_H
#define _PERMISSIONS_H

#include <types.h>


#define PERM_OWNER_R  0400
#define PERM_OWNER_W  0200
#define PERM_OWNER_X  0100
#define PERM_GROUP_R  0040
#define PERM_GROUP_W  0020
#define PERM_GROUP_X  0010
#define PERM_OTHER_R  0004
#define PERM_OTHER_W  0002
#define PERM_OTHER_X  0001


#define PERM_DEFAULT_FILE 0644   
#define PERM_DEFAULT_DIR  0755   
#define PERM_EXEC_FILE    0755   


bool perm_check(uint32_t uid, uint32_t gid,
                uint32_t uid_owner, uint32_t gid_owner,
                uint16_t mode, uint8_t requested);


void perm_to_string(uint16_t mode, char *out);

#endif 
