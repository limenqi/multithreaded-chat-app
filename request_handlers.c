#include "request_handlers.h"
#include "udp.h"
#include <string.h>
#include <stdlib.h>
#include "shared_structs.h"
#include <pthread.h>

client_info_t* find_client_by_addr(struct sockaddr_in *addr) {
    client_info_t *cur = client_list_head;
    while (cur != NULL) {
        if (ntohs(cur->addr.sin_port) == ntohs(addr->sin_port) &&
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
    pthread_rwlock_wrlock(&client_list_lock);
    
    client_info_t *new_client = malloc(sizeof(client_info_t));
    strcpy(new_client->name, name);
    new_client->addr = *client_addr;
    new_client->next = client_list_head;
    new_client->muted = NULL;
    client_list_head = new_client;

    struct sockaddr_in addr_copy = new_client->addr;

    pthread_rwlock_unlock(&client_list_lock);

    char reply[BUFFER_SIZE];
    strcpy(reply, "Hi ");
    strcat(reply, name);
    strcat(reply, ", you have successfully connected to the chat!\n");
    udp_socket_write(sd, &addr_copy, reply, BUFFER_SIZE);        
}

void say(int sd, struct sockaddr_in *client_addr, const char *message) {
    pthread_rwlock_rdlock(&client_list_lock);
    
    client_info_t *sender = find_client_by_addr(client_addr);
    if (sender == NULL) {
        perror("client not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    char reply[BUFFER_SIZE];
    strcpy(reply, sender->name);
    strcat(reply, ": ");
    strcat(reply, message);
    
    client_info_t* receiver = client_list_head;
    while (receiver != NULL) {
        if (!check_muted(receiver, sender->name)) {
            udp_socket_write(sd, &receiver->addr, reply, BUFFER_SIZE);
        }
        receiver = receiver->next;
    }

    pthread_rwlock_unlock(&client_list_lock);
}

void sayto(int sd, struct sockaddr_in *client_addr, const char *content) {
    pthread_rwlock_rdlock(&client_list_lock);
    
    char buffer[BUFFER_SIZE];
    strcpy(buffer, content);

    char *saveptr;
    char *target = strtok_r(buffer, " ", &saveptr);
    char *message = strtok_r(NULL, "", &saveptr);
    
    if (!target || !message) {
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }


    client_info_t *sender = find_client_by_addr(client_addr);
    if (sender == NULL) {
        perror("client not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }
    
    client_info_t *receiver = find_client_by_name(target);
    if (receiver == NULL) {
        perror("target not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }
    
    if (check_muted(receiver, sender->name)) {
        pthread_rwlock_unlock(&client_list_lock);
        return;  // receiver muted sender
    }
   
    
    char reply[BUFFER_SIZE];
    strcpy(reply, sender->name);
    strcat(reply, ": ");
    strcat(reply, message);
    
    udp_socket_write(sd, &receiver->addr, reply, BUFFER_SIZE); 

    pthread_rwlock_unlock(&client_list_lock);
}

void disconn(int sd, struct sockaddr_in *client_addr) {
    // client_info_t *del = find_client_by_addr(client_addr);  // optional helper use
    pthread_rwlock_wrlock(&client_list_lock);
    
    client_info_t* cur = client_list_head;
    char reply[BUFFER_SIZE];
    int found_name=0;

    if (cur != NULL &&
        cur->addr.sin_port==client_addr->sin_port &&
        cur->addr.sin_addr.s_addr==client_addr->sin_addr.s_addr) {

            client_list_head=cur->next;

            pthread_rwlock_unlock(&client_list_lock);

            strcpy(reply, "You have disconnected.\n");
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
            
            pthread_rwlock_unlock(&client_list_lock);
            
            strcpy(reply, "You have disconnected.\n");
            udp_socket_write(sd, &delete->addr, reply, BUFFER_SIZE);
            free(delete);                       
            return;
        }
        
        cur=cur->next;
        
    }
    
    pthread_rwlock_unlock(&client_list_lock);
    
    if(found_name==0){
        perror("client not connected");
        return;
    }
}

void mute(int sd, struct sockaddr_in *client_addr, const char *target) {
    pthread_rwlock_wrlock(&client_list_lock);
    
    client_info_t *sender = find_client_by_addr(client_addr);  
    if (sender == NULL) {
        perror("client not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }
    
    
    client_info_t *to_mute=find_client_by_name(target);

    if(to_mute==NULL){
        char reply[BUFFER_SIZE];
        strcpy(reply, target);
        strcat(reply, " not found.\n");
        udp_socket_write(sd, &sender->addr, reply, BUFFER_SIZE);     
        
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    mute_target_t *cur = sender->muted;
    while (cur) {
        if (strcmp(cur->name, target) == 0) {
            char reply[BUFFER_SIZE];
            strcpy(reply, "target already muted\n");
            udp_socket_write(sd, &sender->addr, reply, BUFFER_SIZE);

            pthread_rwlock_unlock(&client_list_lock);
            return;
        }

        cur = cur->next;
    }

    mute_target_t *new = malloc(sizeof(mute_target_t));
    strcpy(new->name, target);
    new->next = sender->muted;
    sender->muted = new;

    pthread_rwlock_unlock(&client_list_lock);
}

void unmute(int sd, struct sockaddr_in *client_addr, const char *target) {
    pthread_rwlock_wrlock(&client_list_lock);
    
    client_info_t *sender = find_client_by_addr(client_addr);

    if (sender == NULL) {
        perror("client not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    mute_target_t *cur = sender->muted;
    
    if (cur != NULL && strcmp(cur->name, target) == 0) {
        sender->muted = cur->next;
        free(cur);
       
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    while (cur != NULL && cur->next != NULL) {
        if (strcmp(cur->next->name, target) == 0) {
            mute_target_t *to_unmute = cur->next;
            cur->next = to_unmute->next;
            free(to_unmute);
            
            pthread_rwlock_unlock(&client_list_lock);
            return;
        }
        cur = cur->next;
    }

    pthread_rwlock_unlock(&client_list_lock);
}

void rename_client(int sd, struct sockaddr_in *client_addr, const char *newname) {
    pthread_rwlock_wrlock(&client_list_lock);
    
    client_info_t *client = find_client_by_addr(client_addr);
    if (client == NULL) {
        perror("client not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    strcpy(client->name, newname);
    struct sockaddr_in addr_copy = client->addr;
    
    pthread_rwlock_unlock(&client_list_lock);  
    
    char reply[BUFFER_SIZE];
    strcpy(reply, "You are now known as");
    strcat(reply, newname);
    strcat(reply, "\n");
    udp_socket_write(sd, &addr_copy, reply, BUFFER_SIZE);

}

void kick(int sd, struct sockaddr_in *client_addr, const char *target) {
    pthread_rwlock_wrlock(&client_list_lock);
    
    if (ntohs(client_addr->sin_port) != ADMIN_PORT) {
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    client_info_t *to_kick = find_client_by_name(target);
    if (to_kick == NULL) {
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    client_info_t *cur = client_list_head;
    
    if (cur == to_kick) {
        client_list_head = cur->next;
        
        pthread_rwlock_unlock(&client_list_lock);
        
        char msg[BUFFER_SIZE];
        strcpy(msg, "You have been kicked.\n");
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

            pthread_rwlock_unlock(&client_list_lock);

            // tell kicked user
            char msg[BUFFER_SIZE];
            strcpy(msg, "You have been kicked.\n");
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

    pthread_rwlock_unlock(&client_list_lock);
}