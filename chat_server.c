#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "udp.h"

typedef struct {
    int sd;
} listener_args_t;

typedef struct client_info {
    char name[64];
    struct sockaddr_in addr;
    struct client_info *next;
} client_info_t;

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

void *listener_thread(void *arg, ) 
{
        int sd = udp_socket_open(SERVER_PORT);

        assert(sd > -1);

        // Server main loop
        while (1) 
        {
            // Storage for request and response messages
            char client_request[BUFFER_SIZE], server_response[BUFFER_SIZE];

            // Demo code (remove later)
            printf("Server is listening on port %d\n", SERVER_PORT);

            // Variable to store incoming client's IP address and port
            struct sockaddr_in client_address;
        
            // This function reads incoming client request from
            // the socket at sd.
            // (See details of the function in udp.h)
            int rc = udp_socket_read(sd, &client_address, client_request, BUFFER_SIZE);

            // Successfully received an incoming request
            if (rc > 0)
            {
                // Demo code (remove later)
                strcpy(server_response, "Hi, the server has received: ");
                strcat(server_response, client_request);
                strcat(server_response, "\n");

                // This function writes back to the incoming client,
                // whose address is now available in client_address, 
                // through the socket at sd.
                // (See details of the function in udp.h)
                rc = udp_socket_write(sd, &client_address, server_response, BUFFER_SIZE);

                // Demo code (remove later)
                printf("Request served...\n");
            }
       
        }    
    
}

void *service_thread(void *arg){
    
}

int main(int argc, char *argv[])
{


    // This function opens a UDP socket,
    // binding it to all IP interfaces of this machine,
    // and port number SERVER_PORT
    // (See details of the function in udp.h)
    

    return 0;
}