#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "udp.h"

#define CLIENT_PORT 10000

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

typedef struct {
    int sd;
    struct sockaddr_in server_addr;
} sender_args_t;

typedef struct {
    int sd;
} listener_args_t;


void *sender_thread(void *arg)
{
    char client_request[BUFFER_SIZE];
    sender_args_t *args = (sender_args_t *)arg;
    while(fgets(client_request, BUFFER_SIZE, stdin) != NULL) {
        // Send the input line to the server
        udp_socket_write(args->sd, &args->server_addr, client_request, BUFFER_SIZE);
    }
    return NULL;
}

void *listener_thread(void *arg) 
{
    struct sockaddr_in responder_addr;
    char server_response[BUFFER_SIZE];
    while(1){
        int rc = udp_socket_read(((listener_args_t *)arg)->sd, &responder_addr, server_response, BUFFER_SIZE);
        if (rc > 0) {
            printf("Received from server: %s", server_response);
        }
    }
    
}
// client code
int main(int argc, char *argv[])
{
    // This function opens a UDP socket,
    // binding it to all IP interfaces of this machine, and port number CLIENT_PORT.
    pthread_t sender, listener;
    int sd = udp_socket_open(0);
    // Variable to store the server's IP address and port
    // responder will always be the same as the server.
    struct sockaddr_in server_addr;
    // Initializing the server's address.
    // We are currently running the server on localhost (127.0.0.1).
    int rc = set_socket_addr(&server_addr, "127.0.0.1", SERVER_PORT);

    sender_args_t send_args = {sd, server_addr};
    listener_args_t listen_args = {sd};
    pthread_create_w(&sender, NULL, sender_thread, (void *)&send_args);
    pthread_create_w(&listener, NULL, listener_thread, (void *)&listen_args);

    pthread_join(sender, NULL);
    pthread_join(listener, NULL);
    
    
    return 0;
}