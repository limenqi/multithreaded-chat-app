#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include "ui.h"
#include "shared_structs.h"

pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;

static int term_rows;
static int term_cols;
static int chat_top = 1;
static int chat_bottom;
static int chat_height;
static int input_line;
static int next_output_line;

static char chat_buffer[50][BUFFER_SIZE]; // currently supports max 50 rows
static int chat_count = 0;

static struct termios oldt, newt;

void ui_detect_size() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    term_rows = w.ws_row;
    term_cols = w.ws_col;

    chat_top = 3;
    chat_bottom = term_rows - 2;
    chat_height = chat_bottom - chat_top + 1;

    input_line = term_rows;
}

void ui_init() {
    ui_detect_size();
    next_output_line = chat_top;
    
    // clear the screen
    printf("\033[2J");
    printf("\033[H");
    
    // draw separator line
    printf("\033[%d;1H", term_rows - 1);
    for (int i = 0; i < term_cols; i++) {
        printf("-");
    }
    
    fflush(stdout);

    chat_count = 0;
}

void ui_draw_chat() {
    // all chat messages in the chat area
    for (int i = 0; i < chat_height; i++) {
        printf("\033[%d;1H", chat_top + i);
        printf("\033[K");  // clear line first

        if (i < chat_count)
            printf("%s", chat_buffer[i]);
    }
    fflush(stdout);
}

void ui_print_message(const char *msg) {
    pthread_mutex_lock(&ui_mutex);
    char clean_msg[BUFFER_SIZE];
    strncpy(clean_msg, msg, BUFFER_SIZE - 1);
    clean_msg[BUFFER_SIZE - 1] = '\0';

    // remove trailing newline if present
    char *newline = strchr(clean_msg, '\n');
    if (newline) *newline = '\0';

    int len = strlen(clean_msg);
    
    // if message is empty, just refresh input
    if (len == 0) {
        ui_refresh_input();
        return;
    }

    int pos = 0;

    // break message into lines that fit the terminal width
    while (pos < len) {
        int remaining = len - pos;
        int chunk_len = (remaining < term_cols) ? remaining : term_cols;

        // print chunk on the correct row
        printf("\033[%d;1H", next_output_line);
        printf("\033[K");  // clear line 
        
        printf("%.*s", chunk_len, clean_msg + pos);
        
        fflush(stdout);

        next_output_line++;

        // wrap around if we hit the bottom of chat area
        if (next_output_line >= term_rows - 1) {
            next_output_line = chat_top;
        }

        pos += chunk_len;
    }
    
    ui_refresh_input();
    fflush(stdout);

    pthread_mutex_unlock(&ui_mutex);
}

void ui_refresh_input() {
    // move cursor to input line
    printf("\033[%d;1H", input_line);
    printf("\033[K");  
    printf("> ");
    fflush(stdout);
}

void ui_disable_echo() {
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    newt.c_lflag &= ~(ICANON | ECHO);   // disable canonical mode and echo
    newt.c_iflag &= ~(ICRNL);           // disable CR to NL mapping

    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

void ui_enable_echo() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int ui_get_input_line() {
    return input_line;
}