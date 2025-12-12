#ifndef SHARED_STRUCTS_H
#define SHARED_STRUCTS_H
#define ADMIN_PORT 6666

#define BUFFER_SIZE 1024
#define SERVER_PORT 12000

#define HISTORY_SIZE 15

#include <netinet/in.h>
#include <pthread.h>
#include <time.h>

typedef struct mute_target{
    char name[64];
    struct mute_target *next;
} mute_target_t;
typedef struct client_info {
    char name[64];
    struct sockaddr_in addr;
    mute_target_t *muted;
    struct client_info *next;
    time_t last_active;
    int was_pinged;
    struct client_info *prev_active;
    struct client_info *next_active;
} client_info_t;

typedef struct {
    int sd;
    struct sockaddr_in client_addr;
    char type[32];
    char request[BUFFER_SIZE];
} service_args_t;

extern client_info_t *client_list_head;
extern pthread_rwlock_t client_list_lock;

extern client_info_t *active_head;
extern client_info_t *active_tail;

typedef struct{
    char messages[HISTORY_SIZE][BUFFER_SIZE];
    int start;
    int count;
    pthread_mutex_t lock;
} history_buffer_t;



extern history_buffer_t history;
#endif