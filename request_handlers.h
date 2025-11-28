#ifndef REQUEST_HANDLERS_H
#define REQUEST_HANDLERS_H

#include <netinet/in.h>

void conn(int sd, struct sockaddr_in *client_addr, const char *name);
void say(int sd, struct sockaddr_in *client_addr, const char *message);
void sayto(int sd, struct sockaddr_in *client_addr, const char *content);
void disconn(int sd, struct sockaddr_in *client_addr);
void mute(int sd, struct sockaddr_in *client_addr, const char *target);
void unmute(int sd, struct sockaddr_in *client_addr, const char *target);
void rename(int sd, struct sockaddr_in *client_addr, const char *newname);
void kick(int sd, struct sockaddr_in *client_addr, const char *target);
#endif