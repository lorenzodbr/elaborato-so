#ifndef GLOBALS_H
#define GLOBALS_H

#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <termios.h>

#include "data.h"

// --------- GAME STRUCTURES ---------

typedef struct {
    int row;
    int col;
} move_t;

typedef struct {
    int matrix[MATRIX_SIZE];
    pid_t pids[PID_ARRAY_LEN];
    char usernames[USERNAMES_ARRAY_LEN][USERNAME_MAX_LEN + 1];
    int result;
    int autoplay;
    char symbols[SYMBOLS_ARRAY_LEN];
    int timeout;
    char client_path[PATH_MAX];
} tris_game_t;

void print_header_server();
void print_header_client();
void print_welcome_message_server();
void print_welcome_message_client(char*);
void print_loading_message();
void print_and_flush(const char*);
void print_spaces(int);
void* timeout_print_handler(void*);
void start_timeout_print(pthread_t*, int*);
void stop_timeout_print(pthread_t);
void print_loading_complete_message();
void print_board(int*, char, char);
void print_symbol(char, int, char*);
void print_timeout(int);
void print_error(const char*);
void print_success(const char*);
void errexit(const char*);
void clear_screen_server();
void clear_screen_client();
void* loading_spinner();
void start_loading_spinner(pthread_t*);
void stop_loading_spinner(pthread_t*);
int digits(int);
int max(int, int);
int min(int, int);
int set_input(struct termios*);
void ignore_previous_input();
bool init_output_settings(struct termios*, struct termios*);
void init_board(int*);
void init_pids(int*);
int record_join(tris_game_t*, int, char*, int);
void set_pid_at(int, int*, int, int);
void record_quit(tris_game_t*, int);
int get_pid_at(int*, int);
bool is_valid_move(int*, char*, move_t*);
int is_game_ended(int*);
int minimax(int*, int, bool);
void chooseBestMove(int*, int);
void chooseRandomMove(int*, int);
void chooseRandomOrBestMove(int*, int, int);
void chooseNextMove(int*, int, int, int);

#endif