#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H
#define ADMIN_PORT 6666
#include <netinet/in.h>

typedef struct mute_target{
    char name[64];
    struct mute_target *next;
} mute_target_t;
typedef struct client_info {
    char name[64];
    struct sockaddr_in addr;
    mute_target_t *muted;
    struct client_info *next;
} client_info_t;



extern client_info_t *client_list_head;

#endif