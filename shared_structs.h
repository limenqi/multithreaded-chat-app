#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H

#include <netinet/in.h>

typedef struct client_info {
    char name[64];
    struct sockaddr_in addr;
    struct client_info *next;
} client_info_t;

extern client_info_t *client_list_head;

#endif