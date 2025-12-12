#ifndef UI_H
#define UI_H

#include<pthread.h>

extern pthread_mutex_t ui_mutex;
void ui_init();
void ui_print_message(const char *msg);
void ui_refresh_input();
void ui_set_username(const char *name);
void ui_disable_echo();
void ui_enable_echo();
int ui_get_input_line();
#endif
