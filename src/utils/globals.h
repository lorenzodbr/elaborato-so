/************************************
 * VR487434 - Lorenzo Di Berardino
 * VR486588 - Filippo Milani
 * 09/05/2024
 *************************************/

#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "semaphores/semaphores.h"

typedef struct {
    int row;
    int col;
} move_t;

typedef struct {
    int matrix[MATRIX_SIZE];
    pid_t pids[PID_ARRAY_LEN];
    char usernames[USERNAMES_ARRAY_LEN][USERNAME_MAX_LEN];
    int result;
    int autoplay;
    char symbols[SYMBOLS_ARRAY_LEN];
    int timeout;
} tris_game_t;

void print_header_server()
{
    printf(CLEAR_SCREEN);
    printf(TRIS_ASCII_ART_SERVER);
}

void print_header_client()
{
    printf(CLEAR_SCREEN);
    printf(BOLD FCYN TRIS_ASCII_ART_CLIENT NO_BOLD FNRM);
}

void print_welcome_message_server()
{
    print_header_server();
    printf(CREDITS);
    printf(NO_BOLD FNRM);
}

void print_welcome_message_client(char* username)
{
    print_header_client();
    printf(CREDITS);
    printf(WELCOME_CLIENT_MESSAGE, username);
}

void print_loading_message()
{
    printf(LOADING_MESSAGE FGRN);

#if DEBUG
    printf(MY_PID_MESSAGE, getpid());
#endif
}

void print_and_flush(const char* msg)
{
    printf("%s", msg);
    fflush(stdout);
}

void* loading_spinner()
{
    while (1) {
        print_and_flush("\b|");
        usleep(100000);
        print_and_flush("\b/");
        usleep(100000);
        print_and_flush("\b-");
        usleep(100000);
        print_and_flush("\b\\");
        usleep(100000);
    }

    return NULL;
}

void start_loading_spinner(pthread_t* tid)
{
    pthread_create(tid, NULL, loading_spinner, NULL);
}

void stop_loading_spinner(pthread_t* tid)
{
    if (*tid != 0) {
        pthread_cancel(*tid);
        print_and_flush("\b \b");
        *tid = 0;
    }
}

int digits(int n)
{
    if (n < 0)
        return digits((n == INT_MIN) ? INT_MAX : -n);
    if (n < 10)
        return 1;
    return 1 + digits(n / 10);
}

void print_spaces(int n)
{
    for (int i = 0; i < n; i++) {
        printf(" ");
    }

    fflush(stdout);
}

void* timeout_print_handler(void* timeout)
{
    int timeoutValue = *(int*)timeout;
    int originalDigits = digits(timeoutValue) + (timeoutValue % 10 != 0), newDigits;

    while (timeoutValue > 0) {
        newDigits = digits(timeoutValue);
        printf("\x1b[s\r");
        print_spaces(originalDigits - newDigits);
        printf("(%d)\x1b[u", timeoutValue--);
        fflush(stdout);

        sleep(1);
    }

    return NULL;
}

void start_timeout_print(pthread_t* tid, int* timeout)
{
    pthread_create(tid, NULL, timeout_print_handler, timeout);
}

void stop_timeout_print(pthread_t tid)
{
    if (tid == 0) {
        return;
    }

    pthread_cancel(tid);
}

int set_input(struct termios* policy)
{
    return tcsetattr(STDOUT_FILENO, TCSANOW, policy);
}

void ignore_previous_input()
{
    tcflush(STDIN_FILENO, TCIFLUSH);
}

bool init_output_settings(struct termios* with_echo, struct termios* without_echo)
{
    if (tcgetattr(STDOUT_FILENO, with_echo) != 0) {
        return false;
    }

    memcpy(without_echo, with_echo, sizeof(struct termios));
    without_echo->c_lflag &= ~ECHO;

    return true;
}

void print_loading_complete_message()
{
    printf(LOADING_COMPLETE_MESSAGE);
}

void init_board(int* matrix)
{
    for (int i = 0; i < MATRIX_SIDE_LEN; i++) {
        for (int j = 0; j < MATRIX_SIDE_LEN; j++) {
            matrix[i * MATRIX_SIDE_LEN + j] = 0;
        }
    }

#if DEBUG
    printf(MATRIX_INITIALIZED_MESSAGE);
#endif
}

void print_board(int* matrix, char player_one_symbol, char player_two_symbol)
{
    int cell;

    printf(MATRIX_TOP_ROW);
    for (int i = 0; i < MATRIX_SIDE_LEN; i++) {
        printf("  %d  ", i + 1);

        for (int j = 0; j < MATRIX_SIDE_LEN; j++) {
            cell = matrix[i * MATRIX_SIDE_LEN + j];

            switch (cell) {
            case 0:
                printf(EMPTY_CELL);
                break;
            case 1:
                printf(PLAYER_ONE_COLOR BOLD " %c" FNRM NO_BOLD, player_one_symbol);
                break;
            case 2:
                printf(PLAYER_TWO_COLOR BOLD " %c" FNRM NO_BOLD, player_two_symbol);
                break;
            }

            if (j < MATRIX_SIDE_LEN - 1) {
                printf(VERTICAL_SEPARATOR);
            }
        }
        if (i < MATRIX_SIDE_LEN - 1) {
            printf(HORIZONTAL_SEPARATOR);
        }
    }
    printf("\n\n");
}

void print_symbol(char symbol, int player_index, char* username)
{
    printf(YOUR_SYMBOL_IS_MESSAGE, username, player_index == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, symbol);
}

void print_timeout(int timeout)
{
    printf(INFO_CHAR);
    printf(timeout == 0 ? INFINITE_TIMEOUT_MESSAGE : TIMEOUT_MESSAGE, timeout);
}

void print_error(const char* msg)
{
    print_and_flush(FRED ERROR_CHAR);
    print_and_flush(msg);
    print_and_flush(FNRM);
}

void clear_screen_server()
{
    print_header_server();
}

void clear_screen_client()
{
    print_header_client();
}

void print_success(const char* msg)
{
    print_and_flush(FGRN SUCCESS_CHAR);
    print_and_flush(msg);
}

void init_pids(int* pids_pointer)
{
    for (int i = 0; i < 3; i++) {
        pids_pointer[i] = 0;
    }
}

// set specified pid to the first empty slot (out of 3)
int record_join(int sem_id, tris_game_t* game, int pid, char* username, int autoplay)
{
    int player_index = TOO_MANY_PLAYERS_ERROR_CODE;

    wait_semaphore(sem_id, PID_LOCK, 1);
    for (int i = 1; i < PID_ARRAY_LEN; i++) {
        if (game->pids[i] == 0) {
            int otherPlayerIndex = i == 1 ? 2 : 1;

            if (strcmp(username, game->usernames[otherPlayerIndex]) == 0) {
                player_index = SAME_USERNAME_ERROR_CODE;
                break;
            }

            if (autoplay != NONE) {
                game->autoplay = autoplay;
            }

            game->pids[i] = pid;
            player_index = i;
            strcpy(game->usernames[i], username);

            break;
        }
    }
    signal_semaphore(sem_id, PID_LOCK, 1);

    return player_index;
}

void set_pid_at(int sem_id, int* pids_pointer, int index, int pid)
{
    wait_semaphore(sem_id, PID_LOCK, 1);
    pids_pointer[index] = pid;
    signal_semaphore(sem_id, PID_LOCK, 1);
}

void record_quit(tris_game_t* game, int index)
{
    int sem_id = get_semaphores(N_SEM);

    wait_semaphore(sem_id, PID_LOCK, 1);
    game->pids[index] = 0;
    free(game->usernames[index]);
    signal_semaphore(sem_id, PID_LOCK, 1);
}

// no need to use semaphores here, as only the server read from this buffer,
// and it does this only after clients have written to it and notified the server
int get_pid(int* pids_pointer, int index)
{
    return pids_pointer[index];
}

bool is_valid_move(int* matrix, char* input, move_t* move)
{
    // the input is valid if it is in the format [A-C or a-c],[1-3]

    if (strlen(input) != 2) {
        return false;
    }

    if (((input[0] < 'A' || input[0] > 'C') && (input[0] < 'a' || input[0] > 'c')) || input[1] < '1' || input[1] > '3') {
        return false;
    }

    move->row = input[0] - 'A';

    if (input[0] >= 'a' && input[0] <= 'c') {
        move->row = input[0] - 'a';
    }

    move->col = input[1] - '1';

    if (matrix[move->col * MATRIX_SIDE_LEN + move->row] != 0) {
        return false;
    }

    return true;
}

int is_game_ended(int* matrix)
{
    // check rows
    for (int i = 0; i < MATRIX_SIDE_LEN; i++) {
        if (matrix[i * MATRIX_SIDE_LEN] != 0 && matrix[i * MATRIX_SIDE_LEN] == matrix[i * MATRIX_SIDE_LEN + 1] && matrix[i * MATRIX_SIDE_LEN] == matrix[i * MATRIX_SIDE_LEN + 2]) {
            return matrix[i * MATRIX_SIDE_LEN];
        }
    }

    // check columns
    for (int i = 0; i < MATRIX_SIDE_LEN; i++) {
        if (matrix[i] != 0 && matrix[i] == matrix[i + MATRIX_SIDE_LEN] && matrix[i] == matrix[i + 2 * MATRIX_SIDE_LEN]) {
            return matrix[i];
        }
    }

    // check diagonals
    if (matrix[0] != 0 && matrix[0] == matrix[4] && matrix[0] == matrix[8]) {
        return matrix[0];
    }

    if (matrix[2] != 0 && matrix[2] == matrix[4] && matrix[2] == matrix[6]) {
        return matrix[2];
    }

    // check draw
    for (int i = 0; i < MATRIX_SIZE; i++) {
        if (matrix[i] == 0) {
            return NOT_FINISHED;
        }
    }

    return DRAW;
}

int max(int a, int b)
{
    return a > b ? a : b;
}

int min(int a, int b)
{
    return a < b ? a : b;
}

int minimax(int* game_matrix, int depth, bool is_maximizing)
{
    int result = is_game_ended(game_matrix);

    if (result == DRAW)
        return DRAW;

    if (result != NOT_FINISHED) {
        return result == PLAYER_TWO ? 1 : -1;
    }

    if (is_maximizing) {
        int best_val = INT_MIN;

        for (int i = 0; i < MATRIX_SIZE; i++) {
            if (game_matrix[i] == 0) {
                game_matrix[i] = PLAYER_TWO;
                best_val = max(best_val, minimax(game_matrix, depth + 1, false));
                game_matrix[i] = 0;
            }
        }

        return best_val;
    } else {
        int best_val = INT_MAX;

        for (int i = 0; i < MATRIX_SIZE; i++) {
            if (game_matrix[i] == 0) {
                game_matrix[i] = PLAYER_ONE;
                best_val = min(best_val, minimax(game_matrix, depth + 1, true));
                game_matrix[i] = 0;
            }
        }

        return best_val;
    }
}

// uses minimax to choose the next best move
void chooseBestMove(int* game_matrix, int player_index)
{
    int best_val = INT_MIN;
    move_t best_move;

    for (int i = 0; i < MATRIX_SIZE; i++) {
        if (game_matrix[i] == 0) {
            game_matrix[i] = player_index;
            int move_val = minimax(game_matrix, 0, false);
            game_matrix[i] = 0;

            if (move_val > best_val) {
                best_move.row = i % MATRIX_SIDE_LEN;
                best_move.col = i / MATRIX_SIDE_LEN;
                best_val = move_val;
            }
        }
    }

    game_matrix[best_move.row + best_move.col * MATRIX_SIDE_LEN] = player_index;
}

void chooseRandomMove(int* game_matrix, int player_index)
{
    int random_index;

    do {
        random_index = rand() % (MATRIX_SIZE);
    } while (game_matrix[random_index] != 0);

    game_matrix[random_index] = player_index;
}

void chooseRandomOrBestMove(int* game_matrix, int player_index, int cycles)
{
    if (cycles % 2 != 0)
        chooseBestMove(game_matrix, player_index);
    else
        chooseRandomMove(game_matrix, player_index);
}

void chooseNextMove(int* game_matrix, int difficulty, int player_index, int cycles)
{
    switch (difficulty) {
    case EASY:
        chooseRandomMove(game_matrix, player_index);
        return;
    case MEDIUM:
        chooseRandomOrBestMove(game_matrix, player_index, cycles);
        return;
    case HARD:
        chooseBestMove(game_matrix, player_index);
    }
}