#include "request_handlers.h"
#include "udp.h"
#include <string.h>
#include <stdlib.h>
#include "shared_structs.h"

void conn(int sd, struct sockaddr_in *client_addr, const char *name) {
    client_info_t *new_client = malloc(sizeof(client_info_t));
    strcpy(new_client->name, name);
    new_client->addr = *client_addr;
    new_client->next = client_list_head;
    client_list_head = new_client;
    char reply[BUFFER_SIZE];
    strcpy(reply, "Hi ");
    strcat(reply, new_client->name);
    strcat(reply, ", you have successfully connected to the chat!");
    udp_socket_write(sd, &new_client->addr, reply, BUFFER_SIZE);        
}

void say(int sd, struct sockaddr_in *client_addr, const char *message) {
    client_info_t* cur = client_list_head;
    char reply[BUFFER_SIZE];
    int found_name=0;
    while(cur!=NULL){
        if (cur->addr.sin_port == client_addr->sin_port &&
            cur->addr.sin_addr.s_addr == client_addr->sin_addr.s_addr){
                strcpy(reply, cur->name);
                strcat(reply, ": ");
                strcat(reply, message);
                found_name =1;
                break;
        }else{
            cur=cur->next;
        }
    }
    if(found_name==0){
        perror("client not connected");
        return;
    }
    client_info_t* receiver = client_list_head;
    while (receiver != NULL) {
        udp_socket_write(sd, &receiver->addr, reply, BUFFER_SIZE);
        receiver = receiver->next;
    }
}

void sayto(int sd, struct sockaddr_in *client_addr, const char *content) {
    char buffer[BUFFER_SIZE];
    strcpy(buffer, content);
    char *target = strtok(buffer, " ");
    char *message = strtok(NULL, "");
    client_info_t* cur = client_list_head;
    client_info_t* sender = client_list_head;
    char reply[BUFFER_SIZE];
    int found_name=0;
    while(cur!=NULL){
        if (cur->addr.sin_port == client_addr->sin_port &&
            cur->addr.sin_addr.s_addr == client_addr->sin_addr.s_addr){
                sender = cur;
                found_name=1;
                break;
        }else{
            cur=cur->next;
        }
    }
    if(found_name==0){
        perror("client not connected");
        return;
    }
    cur = client_list_head;
    int found_receiver=0;
    client_info_t* receiver = NULL;
    while(cur!=NULL){
        if (strcmp(cur->name, target) == 0){
                found_receiver =1;
                receiver = cur;
                break;
        }else{
            cur=cur->next;
        }
    }
    if(found_receiver==0){
        perror("target not connected");
        return;
    }
    strcpy(reply, sender->name);
    strcat(reply, ": ");
    strcat(reply, message);
    udp_socket_write(sd, &receiver->addr, reply, BUFFER_SIZE);

}

void disconn(int sd, struct sockaddr_in *client_addr) {
    client_info_t* cur = client_list_head;
    char reply[BUFFER_SIZE];
    int found_name=0;
    if(cur->addr.sin_port==client_addr->sin_port&&
        cur->addr.sin_addr.s_addr==client_addr->sin_addr.s_addr){
            client_list_head=cur->next;
            char reply[BUFFER_SIZE];
            strcpy(reply, "You have disconnected.");
            udp_socket_write(sd, &cur->addr, reply, BUFFER_SIZE);
            free(cur);
            return;
        }
    while(cur!=NULL && cur->next != NULL){
        if(cur->next->addr.sin_port==client_addr->sin_port&&
        cur->next->addr.sin_addr.s_addr==client_addr->sin_addr.s_addr){
            client_info_t* delete = cur->next;
            cur->next=cur->next->next;
            found_name=1;
            char reply[BUFFER_SIZE];
            strcpy(reply, "You have disconnected.");
            udp_socket_write(sd, &delete->addr, reply, BUFFER_SIZE);
            free(delete);                       
            return;
        }else{
            cur=cur->next;
        }
    }
    if(found_name==0){
        perror("client not connected");
        return;
    }
}

void mute(int sd, struct sockaddr_in *client_addr, const char *target) {
    
}

void unmute(int sd, struct sockaddr_in *client_addr, const char *target) {
    
}

void rename(int sd, struct sockaddr_in *client_addr, const char *newname) {
    
}

void kick(int sd, const char *target) {
    
}