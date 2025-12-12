#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include "request_handlers.h"
#include "shared_structs.h"
//To compile this file, run "gcc chat_server.c request_handlers.c -o chat_server -lpthread"

#define INACTIVITY_THRESHOLD 300 // 5 minutes
#define PING_TIMEOUT 30 

int udp_socket_open(int port);
int udp_socket_read(int sd, struct sockaddr_in *addr, char *buffer, int n);
int udp_socket_write(int sd, struct sockaddr_in *addr, char *buffer, int n);
int set_socket_addr(struct sockaddr_in *addr, const char *host, int port);

client_info_t *client_list_head = NULL; 
pthread_rwlock_t client_list_lock = PTHREAD_RWLOCK_INITIALIZER;

history_buffer_t history = {
    .start = 0,
    .count = 0,
    .lock = PTHREAD_MUTEX_INITIALIZER
};

client_info_t *active_head = NULL;
client_info_t *active_tail = NULL;

static int server_sd = -1;

void pthread_create_w(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg)
{
    int rc = pthread_create(thread, attr, start_routine, arg);
    if (rc != 0) {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
}

void pthread_join_w(pthread_t thread, void **retval)
{
    int rc = pthread_join(thread, retval);
    if (rc != 0) {
        fprintf(stderr, "pthread_join failed: %s\n", strerror(rc));
        exit(EXIT_FAILURE);
    }
}

// helper function
static void mark_client_active(struct sockaddr_in *addr)
{
    time_t now = time(NULL);

    pthread_rwlock_wrlock(&client_list_lock);

    client_info_t *cur = client_list_head;
    while (cur != NULL) {
        if (ntohs(cur->addr.sin_port) == ntohs(addr->sin_port) &&
            cur->addr.sin_addr.s_addr == addr->sin_addr.s_addr) {

            cur->last_active = now;
            break;
        }
        cur = cur->next;
    }

    pthread_rwlock_unlock(&client_list_lock);
}


static client_info_t *find_least_recently_active_locked(void)
{
    client_info_t *cur = client_list_head;
    client_info_t *oldest = NULL;

    while (cur != NULL) {
        
        if (ntohs(cur->addr.sin_port) == ADMIN_PORT) {
            cur = cur->next;
            continue;
        }        



        if (cur->last_active != 0) {
            if (oldest == NULL || cur->last_active < oldest->last_active) {
                oldest = cur;
            }
        }
        cur = cur->next;
    }

    return oldest;
}

void *service_thread(void *arg){
    service_args_t* args = (service_args_t*)arg;
    if (strcmp(args->type, "conn") == 0) {
        conn(args->sd, &args->client_addr,args->request);
        mark_client_active(&args->client_addr);
    }
    else if (strcmp(args->type, "say") == 0) {
        say(args->sd, &args->client_addr,args->request);
        mark_client_active(&args->client_addr);
    }
    else if (strcmp(args->type, "sayto") == 0) {
        sayto(args->sd, &args->client_addr,args->request);
        mark_client_active(&args->client_addr);
    }
    else if (strcmp(args->type, "disconn") == 0) {
        disconn(args->sd, &args->client_addr);
    }
    else if (strcmp(args->type, "mute") == 0) {
        mute(args->sd, &args->client_addr, args->request);
        mark_client_active(&args->client_addr);
    }
    else if (strcmp(args->type, "unmute") == 0) {
        unmute(args->sd, &args->client_addr, args->request);
        mark_client_active(&args->client_addr);
    }
    else if (strcmp(args->type, "rename") == 0) {
        rename_client(args->sd, &args->client_addr, args->request);
        mark_client_active(&args->client_addr);
    }
    else if (strcmp(args->type, "kick") == 0) {
        kick(args->sd, &args->client_addr, args->request);   
        mark_client_active(&args->client_addr); 
    }
    else if (strcmp(args->type, "ret-ping") == 0) {
        ret_ping(args->sd, &args->client_addr);
    } 
    else{
        fprintf(stderr, "Unknown request type: %s\n", args->type);
    } 
    free(args);
    return NULL;
}

void *listener_thread(void *arg) {
        int sd = udp_socket_open(SERVER_PORT);
        assert(sd > -1);
        server_sd = sd;
        // listener thread main loop
        while (1){ 
            // Storage for request and response messages
            char client_request[BUFFER_SIZE];
            // Variable to store incoming client's IP address and port
            struct sockaddr_in client_address;
            int rc = udp_socket_read(sd, &client_address, client_request, BUFFER_SIZE);
            if (rc > 0){
                client_request[strcspn(client_request, "\n")] = '\0';
                char *type = strtok(client_request, "$");
                char *content = strtok(NULL, "$");
                if (type == NULL) {
                    perror("invalid request format");
                    continue;
                }
                if(content==NULL){
                    content="";
                }
                service_args_t *args = malloc(sizeof(service_args_t));
                args->sd = sd;
                args->client_addr = client_address;
                strcpy(args->type, type);
                strcpy(args->request, content);
                
                pthread_t service;
                pthread_create_w(&service, NULL, service_thread, args);
                pthread_detach(service);
            }
        }    
}

void *monitor_thread(void *arg)
{
    (void)arg;

    while (1) {
        sleep(5); // check every 5 seconds

        // wait until listener has opened the socket
        if (server_sd < 0) {
            continue;
        }

        time_t now = time(NULL);

        // find the least recently active client
        pthread_rwlock_rdlock(&client_list_lock);
        client_info_t *oldest = find_least_recently_active_locked();

        if (oldest == NULL) {
            // no clients OR it might be none with valid last_active
            pthread_rwlock_unlock(&client_list_lock);
            continue;
        }

        double idle = difftime(now, oldest->last_active);
        struct sockaddr_in oldest_addr = oldest->addr; // copy address before unlocking

        pthread_rwlock_unlock(&client_list_lock);

        if (idle < INACTIVITY_THRESHOLD) {
            continue;
        }

        // send ping$ message to the least recently active client
        const char *ping_msg = "[Server]: You have been inactive. Please send a message or ret-ping$ to stay connected.\n";
        if (ntohs(oldest_addr.sin_port) != ADMIN_PORT) {
            udp_socket_write(server_sd, &oldest_addr,
                            (char *)ping_msg, (int)strlen(ping_msg));
        }

        // wait for ret-ping$ response 
        sleep(PING_TIMEOUT);

        now = time(NULL);

        pthread_rwlock_rdlock(&client_list_lock);

        client_info_t *cur = client_list_head;
        client_info_t *current_oldest = find_least_recently_active_locked();

        int still_same_client = 0;
        if (current_oldest != NULL &&
            ntohs(current_oldest->addr.sin_port) == ntohs(oldest_addr.sin_port) &&
            current_oldest->addr.sin_addr.s_addr == oldest_addr.sin_addr.s_addr) {

            double idle_now = difftime(now, current_oldest->last_active);
            if (idle_now >= INACTIVITY_THRESHOLD) {
                still_same_client = 1;
            }
        }

        pthread_rwlock_unlock(&client_list_lock);

        if (still_same_client) {
            disconn(server_sd, &oldest_addr);
        }
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    pthread_rwlock_init (&client_list_lock, NULL);
    
    pthread_t listener;
    pthread_t monitor;

    pthread_create_w(&listener, NULL, listener_thread, NULL);
    pthread_create_w(&monitor, NULL, monitor_thread, NULL);
    
    // This function opens a UDP socket,
    // binding it to all IP interfaces of this machine,
    // and port number SERVER_PORT
    // (See details of the function in udp.h)
    
    pthread_join(listener, NULL);
    pthread_join(monitor, NULL);

    return 0;
}