#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H
#define ADMIN_PORT 6666

#define BUFFER_SIZE 1024
#define SERVER_PORT 12000
#include <netinet/in.h>
#include <pthread.h>

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

typedef struct {
    int sd;
    struct sockaddr_in client_addr;
    char type[32];
    char request[BUFFER_SIZE];
} service_args_t;

extern client_info_t *client_list_head;
extern pthread_rwlock_t client_list_lock;

#endif