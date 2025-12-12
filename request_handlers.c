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

void add_to_history(const char *msg) {
    pthread_mutex_lock(&history.lock);

    int index = (history.start + history.count) % HISTORY_SIZE;
    strncpy(history.messages[index], msg, BUFFER_SIZE - 1);
    history.messages[index][BUFFER_SIZE - 1] = '\0';

    if (history.count < HISTORY_SIZE) {
        history.count++;
    } else {
        history.start = (history.start + 1) % HISTORY_SIZE;
    }

    pthread_mutex_unlock(&history.lock);
}

static void strip_leading_whitespace(char *s)
{
    char *start = s;

    while (*start == ' ' || *start == '\t') {
        start++;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

void conn(int sd, struct sockaddr_in *client_addr, const char *name) {
    char clean_name[BUFFER_SIZE];
    strncpy(clean_name, name, BUFFER_SIZE - 1);
    clean_name[BUFFER_SIZE - 1] = '\0';
    strip_leading_whitespace(clean_name);

    if (clean_name[0] == '\0') {
        char reply[BUFFER_SIZE];
        snprintf(reply, BUFFER_SIZE, "Invalid name.\n");
        udp_socket_write(sd, client_addr, reply, strlen(reply));
        return;
    }
    
    pthread_rwlock_wrlock(&client_list_lock);
    
    if (find_client_by_addr(client_addr) != NULL) {
    pthread_rwlock_unlock(&client_list_lock);

    char msg[BUFFER_SIZE];
    snprintf(msg, BUFFER_SIZE,
             "You are already connected. Use rename$ to change your name.\n");
    udp_socket_write(sd, client_addr, msg, strlen(msg));
    return;
}

    if (find_client_by_name(clean_name) != NULL) {
        pthread_rwlock_unlock(&client_list_lock);

        char reply[BUFFER_SIZE];
        snprintf(reply, BUFFER_SIZE,
                 "Name '%s' is already in use. Please choose another name.\n",
                 clean_name);
        udp_socket_write(sd, client_addr, reply, strlen(reply));
        return;
    }    

    client_info_t *new_client = malloc(sizeof(client_info_t));
    strcpy(new_client->name, clean_name);
    new_client->addr = *client_addr;
    new_client->next = client_list_head;
    new_client->muted = NULL;
    client_list_head = new_client;
    new_client->last_active = time(NULL);   // NEW
    new_client->prev_active = NULL;         // (not used, but safe)
    new_client->next_active = NULL;


    struct sockaddr_in addr_copy = new_client->addr;

    pthread_rwlock_unlock(&client_list_lock);

    char reply[BUFFER_SIZE];
    snprintf(reply, BUFFER_SIZE,
             "Hi %s, you have successfully connected to the chat!\n",
             clean_name);
    udp_socket_write(sd, &addr_copy, reply, strlen(reply));  
    
    char local_history[HISTORY_SIZE][BUFFER_SIZE];
    int local_count = 0;

    pthread_mutex_lock(&history.lock);

    for (int i = 0; i < history.count; i++) {
        int index = (history.start + i) % HISTORY_SIZE;
        strcpy(local_history[local_count++], history.messages[index]);
    }

    pthread_mutex_unlock(&history.lock);

    /* send history AFTER unlocking */
    for (int i = 0; i < local_count; i++) {
        udp_socket_write(sd, &addr_copy,
                        local_history[i],
                        strlen(local_history[i]));
    }
}

void say(int sd, struct sockaddr_in *client_addr, const char *message) {
    char clean_msg[BUFFER_SIZE];

    strncpy(clean_msg, message, BUFFER_SIZE - 1);
    clean_msg[BUFFER_SIZE - 1] = '\0';
    strip_leading_whitespace(clean_msg);

    struct sockaddr_in targets[64];
    int target_count = 0;    
    
    char sender_name[64];

    pthread_rwlock_rdlock(&client_list_lock);
    
    client_info_t *sender = find_client_by_addr(client_addr);
    if (sender == NULL) {
        perror("client not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    strcpy(sender_name, sender->name);

    client_info_t* receiver = client_list_head;
    while (receiver != NULL) {
        // send to all clients except sender and those who muted sender
        if (receiver != sender && !check_muted(receiver, sender->name)) {
            targets[target_count++] = receiver->addr;
        } 
        receiver = receiver->next;
    }

    pthread_rwlock_unlock(&client_list_lock);

    char reply[BUFFER_SIZE];
    snprintf(reply, BUFFER_SIZE,
             "%s: %s", sender_name, clean_msg);
    
    add_to_history(reply);

    for (int i = 0; i < target_count; i++){
        udp_socket_write(sd, &targets[i], reply, strlen(reply));
    }
}

void sayto(int sd, struct sockaddr_in *client_addr, const char *content) {
    
    char buffer[BUFFER_SIZE];
    char clean_target[BUFFER_SIZE];

    strncpy(buffer, content, BUFFER_SIZE - 1);
    buffer[BUFFER_SIZE - 1] = '\0';

    char *saveptr;
    char *target = strtok_r(buffer, " ", &saveptr);
    char *message = strtok_r(NULL, "", &saveptr);
    
    if (!target || !message) {
        return;
    }

    strncpy(clean_target, target, BUFFER_SIZE - 1);
    clean_target[BUFFER_SIZE - 1] = '\0';
    strip_leading_whitespace(clean_target);
    strip_leading_whitespace(message);

    if (clean_target[0] == '\0' || message[0] == '\0') {
        return;
    }

    struct sockaddr_in receiver_addr;
    char sender_name[64];
    int ok = 0;
    
    pthread_rwlock_rdlock(&client_list_lock);

    client_info_t *sender = find_client_by_addr(client_addr);
    if (sender == NULL) {
        perror("client not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }
    
    client_info_t *receiver = find_client_by_name(clean_target);
    if (receiver == NULL) {
        perror("target not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }
    
    if (check_muted(receiver, sender->name)) {
        pthread_rwlock_unlock(&client_list_lock);
        return;  // receiver muted sender
    }
   
    receiver_addr = receiver->addr;          // copy before unlock
    strcpy(sender_name, sender->name);
    ok = 1;

    pthread_rwlock_unlock(&client_list_lock);

    if (!ok) {
        return;
    }

    char reply[BUFFER_SIZE];
    snprintf(reply, BUFFER_SIZE, "%s: %s", sender_name, message);

    udp_socket_write(sd, &receiver_addr, reply, strlen(reply));
    
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
            udp_socket_write(sd, &cur->addr, reply, strlen(reply));
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
            udp_socket_write(sd, &delete->addr, reply, strlen(reply));
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
    char clean_target[BUFFER_SIZE];

    strncpy(clean_target, target, BUFFER_SIZE - 1);
    clean_target[BUFFER_SIZE - 1] = '\0';
    strip_leading_whitespace(clean_target);

    if (clean_target[0] == '\0') {
        return;
    }
    
    pthread_rwlock_wrlock(&client_list_lock);
    
    client_info_t *sender = find_client_by_addr(client_addr);  
    if (sender == NULL) {
        perror("client not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }
    
    
    client_info_t *to_mute=find_client_by_name(clean_target);

    if(to_mute==NULL){
        char reply[BUFFER_SIZE];
        snprintf(reply, BUFFER_SIZE, "%s not found.\n", clean_target);
        udp_socket_write(sd, &sender->addr, reply, strlen(reply));     
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    mute_target_t *cur = sender->muted;
    while (cur) {
        if (strcmp(cur->name, clean_target) == 0) {
            char reply[BUFFER_SIZE];
            strcpy(reply, "target already muted\n");
            udp_socket_write(sd, &sender->addr, reply, strlen(reply));

            pthread_rwlock_unlock(&client_list_lock);
            return;
        }

        cur = cur->next;
    }

    mute_target_t *new = malloc(sizeof(mute_target_t));
    strcpy(new->name, clean_target);
    new->next = sender->muted;
    sender->muted = new;

    pthread_rwlock_unlock(&client_list_lock);
}

void unmute(int sd, struct sockaddr_in *client_addr, const char *target) {
    char clean_target[BUFFER_SIZE];

    strncpy(clean_target, target, BUFFER_SIZE - 1);
    clean_target[BUFFER_SIZE - 1] = '\0';
    strip_leading_whitespace(clean_target);

    if (clean_target[0] == '\0') {
        return;
    }

    pthread_rwlock_wrlock(&client_list_lock);
    
    client_info_t *sender = find_client_by_addr(client_addr);

    if (sender == NULL) {
        perror("client not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    mute_target_t *cur = sender->muted;
    
    if (cur != NULL && strcmp(cur->name, clean_target) == 0) {
        sender->muted = cur->next;
        free(cur);
       
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    while (cur != NULL && cur->next != NULL) {
        if (strcmp(cur->next->name, clean_target) == 0) {
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
    char clean_name[BUFFER_SIZE];

    strncpy(clean_name, newname, BUFFER_SIZE - 1);
    clean_name[BUFFER_SIZE - 1] = '\0';
    strip_leading_whitespace(clean_name);

    if (clean_name[0] == '\0') {
        char msg[] = "Invalid name.\n";
        udp_socket_write(sd, client_addr, msg, strlen(msg));
        return;
    }

    pthread_rwlock_wrlock(&client_list_lock);
   
    if (find_client_by_name(clean_name) != NULL) {
        pthread_rwlock_unlock(&client_list_lock);

        char reply[BUFFER_SIZE];
        snprintf(reply, BUFFER_SIZE,
                 "Name '%s' is already in use. Please choose another name\n", clean_name);
        udp_socket_write(sd, client_addr, reply, strlen(reply));
        return;
    }    

    client_info_t *client = find_client_by_addr(client_addr);
    if (client == NULL) {
        perror("client not connected");
        pthread_rwlock_unlock(&client_list_lock);
        return;
    }

    strcpy(client->name, clean_name);
    struct sockaddr_in addr_copy = client->addr;
    
    pthread_rwlock_unlock(&client_list_lock);  
    
    char reply[BUFFER_SIZE];
    snprintf(reply, BUFFER_SIZE, "You are now known as %s\n", clean_name);
    udp_socket_write(sd, &addr_copy, reply, strlen(reply));

}

void kick(int sd, struct sockaddr_in *client_addr, const char *target) {
    char clean_target[BUFFER_SIZE];

    strncpy(clean_target, target, BUFFER_SIZE - 1);
    clean_target[BUFFER_SIZE - 1] = '\0';
    strip_leading_whitespace(clean_target);

    if (clean_target[0] == '\0') {
        return;
    }
    
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
        strcpy(msg, "You have been removed from the chat.\n");
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

void ret_ping(int sd, struct sockaddr_in *client_addr) {
    // update last_active timestamp
    time_t now = time(NULL);

    pthread_rwlock_wrlock(&client_list_lock);

    client_info_t *cur = client_list_head;
    while (cur != NULL) {
        if (ntohs(cur->addr.sin_port) == ntohs(client_addr->sin_port) &&
            cur->addr.sin_addr.s_addr == client_addr->sin_addr.s_addr) {

            cur->last_active = now;
            break;
        }
        cur = cur->next;
    }

    pthread_rwlock_unlock(&client_list_lock);

    char msg[] = "[Server]: Activity refreshed. You are still connected.\n";
    udp_socket_write(sd, client_addr, msg, strlen(msg));
}
