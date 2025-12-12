#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include "udp.h"
#include "ui.h"
#include "shared_structs.h"

#define CLIENT_PORT 10000

// global buffer to track current input
static char current_input[BUFFER_SIZE] = "";
static int current_input_pos = 0;
static pthread_mutex_t input_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void redisplay_input() {
    pthread_mutex_lock(&ui_mutex);
    pthread_mutex_lock(&input_mutex);

    printf("\033[%d;1H", ui_get_input_line());
    printf("\033[K");  
    printf("> %s", current_input);
    fflush(stdout);

    pthread_mutex_unlock(&input_mutex);
    pthread_mutex_unlock(&ui_mutex);
}

void *sender_thread(void *arg)
{
    sender_args_t *args = (sender_args_t *)arg;
    
    while (1) {
        int c = getchar();

        pthread_mutex_lock(&input_mutex);
        
        if (c == '\r' || c == '\n') {     // ENTER pressed
            current_input[current_input_pos] = '\0';

            // send only if not empty
            if (current_input_pos > 0) {
                char send_buffer[BUFFER_SIZE];
                snprintf(send_buffer, BUFFER_SIZE, "%s\n", current_input);
                udp_socket_write(args->sd, &args->server_addr, send_buffer, strlen(send_buffer));
            }

            // reset buffer
            current_input_pos = 0;
            current_input[0] = '\0';

            pthread_mutex_unlock(&input_mutex);
            
            // clear the input line
            redisplay_input();
            continue;
        }

        // backspace handling
        if ((c == 127 || c == '\b') && current_input_pos > 0) {
            current_input_pos--;
            current_input[current_input_pos] = '\0';
            pthread_mutex_unlock(&input_mutex);
            redisplay_input();
            continue;
        }

        // normal character input
        if (c >= 32 && c <= 126 && current_input_pos < BUFFER_SIZE - 1) {
            current_input[current_input_pos++] = c;
            current_input[current_input_pos] = '\0';
            pthread_mutex_unlock(&input_mutex);
            redisplay_input();
            continue;
        }
        
        pthread_mutex_unlock(&input_mutex);
    }

    return NULL;
}


void *listener_thread(void *arg) 
{
    struct sockaddr_in responder_addr;
    char server_response[BUFFER_SIZE];
    while(1){
        memset(server_response, 0, BUFFER_SIZE);
        int rc = udp_socket_read(((listener_args_t *)arg)->sd, &responder_addr, server_response, BUFFER_SIZE);
        if (rc > 0) {
            server_response[rc] = '\0';
            ui_print_message(server_response);

            redisplay_input();

            if (strstr(server_response, "You have disconnected") != NULL ||
                strstr(server_response, "You have been removed from the chat") != NULL) {

                printf("Client exiting...\n");
                exit(0);
            }
        }
    }
}
// client code
int main(int argc, char *argv[])
{
    // This function opens a UDP socket,
    // binding it to all IP interfaces of this machine, and port number CLIENT_PORT.
    pthread_t sender, listener;
    int admin = 0;

    if (argc > 1 && strcmp(argv[1], "--admin") == 0) {
        admin = 1;
    }

    int sd = udp_socket_open(admin ? ADMIN_PORT : 0);    // Variable to store the server's IP address and port
    // responder will always be the same as the server.
    struct sockaddr_in server_addr;
    // Initializing the server's address.
    // We are currently running the server on localhost (127.0.0.1).
    int rc = set_socket_addr(&server_addr, "127.0.0.1", SERVER_PORT);

    ui_init();
    ui_disable_echo();

    sender_args_t send_args = {sd, server_addr};
    listener_args_t listen_args = {sd};
    pthread_create_w(&sender, NULL, sender_thread, (void *)&send_args);
    pthread_create_w(&listener, NULL, listener_thread, (void *)&listen_args);

    ui_refresh_input();

    pthread_join(sender, NULL);
    
    ui_enable_echo();

    pthread_detach(listener);
    
    return 0;
}