#include "request_handlers.h"
#include "udp.h"
#include <string.h>
#include <stdlib.h>
#include "shared_structs.h"

client_info_t* find_client_by_addr(struct sockaddr_in *addr) {
    client_info_t *cur = client_list_head;
    while (cur != NULL) {
        if (cur->addr.sin_port == addr->sin_port &&
            cur->addr.sin_addr.s_addr == addr->sin_addr.s_addr) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

client_info_t* find_client_by_name(const char *name) {
    client_info_t *cur = client_list_head;
    while (cur != NULL) {
        if (strcmp(cur->name, name) == 0) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

int check_muted(client_info_t *receiver, const char *sender_name) {
    mute_target_t *cur = receiver->muted;
    while (cur != NULL) {
        if (strcmp(cur->name, sender_name) == 0) {
            return 1;  
        }
        cur = cur->next;
    }
    return 0;   
}

void conn(int sd, struct sockaddr_in *client_addr, const char *name) {
    client_info_t *new_client = malloc(sizeof(client_info_t));
    strcpy(new_client->name, name);
    new_client->addr = *client_addr;
    new_client->next = client_list_head;
    new_client->muted=NULL;
    client_list_head = new_client;
    char reply[BUFFER_SIZE];
    strcpy(reply, "Hi ");
    strcat(reply, new_client->name);
    strcat(reply, ", you have successfully connected to the chat!");
    udp_socket_write(sd, &new_client->addr, reply, BUFFER_SIZE);        
}

void say(int sd, struct sockaddr_in *client_addr, const char *message) {
    client_info_t *sender = find_client_by_addr(client_addr);
    if (sender == NULL) {
        perror("client not connected");
        return;
    }
    char reply[BUFFER_SIZE];
    strcpy(reply, sender->name);
    strcat(reply, ": ");
    strcat(reply, message);
    client_info_t* receiver = client_list_head;
    while (receiver != NULL) {
        if (!is_muted(receiver, sender->name)) {
            udp_socket_write(sd, &receiver->addr, reply, BUFFER_SIZE);
        }
        receiver = receiver->next;
    }
}

void sayto(int sd, struct sockaddr_in *client_addr, const char *content) {
    char buffer[BUFFER_SIZE];
    strcpy(buffer, content);
    char *target = strtok(buffer, " ");
    char *message = strtok(NULL, "");
    client_info_t *sender = find_client_by_addr(client_addr);
    if (sender == NULL) {
        perror("client not connected");
        return;
    }
    char reply[BUFFER_SIZE];
    client_info_t *receiver = find_client_by_name(target);
    if (receiver == NULL) {
        perror("target not connected");
        return;
    }
    strcpy(reply, sender->name);
    strcat(reply, ": ");
    strcat(reply, message);
    if (is_muted(receiver, sender->name)) {
        return;  // receiver muted sender
    }
    udp_socket_write(sd, &receiver->addr, reply, BUFFER_SIZE);

}

void disconn(int sd, struct sockaddr_in *client_addr) {
    // client_info_t *del = find_client_by_addr(client_addr);  // optional helper use
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
    client_info_t *sender = find_client_by_addr(client_addr);  
    client_info_t *to_mute=find_client_by_name(target);
    if(to_mute==NULL){
        char reply[BUFFER_SIZE];
        strcpy(reply, target);
        strcat(reply, " not found.");
        udp_socket_write(sd, &sender->addr, reply, BUFFER_SIZE);        
        return;
    }

    mute_target_t *cur = sender->muted;
    while (cur) {
        if (strcmp(cur->name, target) == 0) {
            char reply[BUFFER_SIZE];
            strcpy(reply, "target already muted");
            udp_socket_write(sd, &sender->addr, reply, BUFFER_SIZE);
            return;
        }
        cur = cur->next;
    }

    mute_target_t *new = malloc(sizeof(mute_target_t));
    strcpy(new->name, target);
    new->next = sender->muted;
    sender->muted = new;
}

void unmute(int sd, struct sockaddr_in *client_addr, const char *target) {
    client_info_t *sender = find_client_by_addr(client_addr);
    mute_target_t *cur = sender->muted;
    if (cur != NULL && strcmp(cur->name, target) == 0) {
        sender->muted = cur->next;
        free(cur);
        return;
    }
    while (cur != NULL && cur->next != NULL) {
        if (strcmp(cur->next->name, target) == 0) {
            mute_target_t *to_unmute = cur->next;
            cur->next = to_unmute->next;
            free(to_unmute);
            return;
        }
        cur = cur->next;
    }
}

void rename_client(int sd, struct sockaddr_in *client_addr, const char *newname) {
    client_info_t *client = find_client_by_addr(client_addr);
    strcpy(client->name, newname);
    char reply[BUFFER_SIZE];
    strcpy(reply, "You are now known as ");
    strcat(reply, newname);
    udp_socket_write(sd, &client->addr, reply, BUFFER_SIZE);

}

void kick(int sd, struct sockaddr_in *client_addr, const char *target) {
    if (client_addr->sin_port != ADMIN_PORT) return;
    client_info_t *to_kick = find_client_by_name(target);
    if (to_kick == NULL) {
        return;
    }
    client_info_t *cur = client_list_head;
    if (cur == to_kick) {
        client_list_head = cur->next;
        char msg[BUFFER_SIZE];
        strcpy(msg, "You have been kicked.");
        udp_socket_write(sd, &to_kick->addr, msg, BUFFER_SIZE);

        // free mute list
        mute_target_t *cur_mute = to_kick->muted;
        while (cur_mute!=NULL) {
            mute_target_t *next = cur_mute->next;
            free(cur_mute);
            cur_mute = next;
        }
        // free client
        free(to_kick);
        return;
    }
    while (cur != NULL && cur->next != NULL) {
        if (cur->next == to_kick) {
            cur->next = to_kick->next;
            // tell kicked user
            char msg[BUFFER_SIZE];
            strcpy(msg, "You have been kicked.");
            udp_socket_write(sd, &to_kick->addr, msg, BUFFER_SIZE);

            // free mute list
            mute_target_t *cur_mute = to_kick->muted;
            while (cur_mute!=NULL) {
                mute_target_t *next = cur_mute->next;
                free(cur_mute);
                cur_mute = next;
            }
            // free client
            free(to_kick);
            return;
        }

        cur = cur->next;
    }
}