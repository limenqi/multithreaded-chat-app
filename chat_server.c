#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "request_handlers.h"
#include "shared_structs.h"
//To compile this file, run "gcc chat_server.c request_handlers.c -o chat_server -lpthread"

int udp_socket_open(int port);
int udp_socket_read(int sd, struct sockaddr_in *addr, char *buffer, int n);
int udp_socket_write(int sd, struct sockaddr_in *addr, char *buffer, int n);
int set_socket_addr(struct sockaddr_in *addr, const char *host, int port);

client_info_t *client_list_head = NULL; 
pthread_rwlock_t client_list_lock = PTHREAD_RWLOCK_INITIALIZER;

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

void *service_thread(void *arg){
    service_args_t* args = (service_args_t*)arg;
    if (strcmp(args->type, "conn") == 0) {
        conn(args->sd, &args->client_addr,args->request);
    }
    if (strcmp(args->type, "say") == 0) {
        say(args->sd, &args->client_addr,args->request);
    }
    if (strcmp(args->type, "sayto") == 0) {
        sayto(args->sd, &args->client_addr,args->request);
    }
    if (strcmp(args->type, "disconn") == 0) {
        disconn(args->sd, &args->client_addr);
    }
    if (strcmp(args->type, "mute") == 0) {
        mute(args->sd, &args->client_addr, args->request);
    }
    if (strcmp(args->type, "unmute") == 0) {
        unmute(args->sd, &args->client_addr, args->request);
    }
    if (strcmp(args->type, "rename") == 0) {
        rename_client(args->sd, &args->client_addr, args->request);
    }
    if (strcmp(args->type, "kick") == 0) {
        kick(args->sd, &args->client_addr, args->request);    
    }
    free(args);
    return NULL;
}

void *listener_thread(void *arg) {
        int sd = udp_socket_open(SERVER_PORT);
        assert(sd > -1);
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
                // rc = udp_socket_write(sd, &client_address, server_response, BUFFER_SIZE);
                pthread_detach(service);
            }
        }    
}


int main(int argc, char *argv[])
{
    pthread_rwlock_init (&client_list_lock, NULL);
    
    pthread_t listener;
    pthread_create_w(&listener, NULL, listener_thread, NULL);
    
    // This function opens a UDP socket,
    // binding it to all IP interfaces of this machine,
    // and port number SERVER_PORT
    // (See details of the function in udp.h)
    
    pthread_join(listener, NULL);

    return 0;
}