/************************************
 * VR487434 - Lorenzo Di Berardino
 * VR486588 - Filippo Milani
 * 09/05/2024
 ************************************/

#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "semaphores/semaphores.h"

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
void clear_screen_server();
void clear_screen_client();
void print_success(const char*);
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
int record_join(int, tris_game_t*, int, char*, int);
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

// --------- MESSAGES ---------

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
    // Print the message and flush the output buffer immediately after
    printf("%s", msg);
    fflush(stdout);
}

void print_spaces(int n)
{
    for (int i = 0; i < n; i++) {
        printf(" ");
    }

    fflush(stdout);
}

/// @brief Function that prints the countdown of the timeout near the prompt
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

/// @brief Starts the thread that prints the countdown
void start_timeout_print(pthread_t* tid, int* timeout)
{
#if PRETTY
    pthread_create(tid, NULL, timeout_print_handler, timeout);
#endif
}

/// @brief Stops the thread that prints the countdown
void stop_timeout_print(pthread_t tid)
{
#if PRETTY
    if (tid == 0) {
        return;
    }

    pthread_cancel(tid);
#endif
}

void print_loading_complete_message()
{
    printf(LOADING_COMPLETE_MESSAGE);
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

/// @brief Print the symbol of the player as a hint
void print_symbol(char symbol, int player_index, char* username)
{
    printf(YOUR_SYMBOL_IS_MESSAGE, username, player_index == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, symbol);
}

void print_timeout(int timeout)
{
    printf(INFO_CHAR);
    printf(timeout == 0 ? INFINITE_TIMEOUT_MESSAGE : TIMEOUT_MESSAGE, timeout);
}

/// @brief Prints an error message with red mark and exits
void print_error(const char* msg)
{
    print_and_flush(FRED ERROR_CHAR);
    print_and_flush(msg);
    print_and_flush(FNRM);
}

/// @brief Prints a success message with green mark
void print_success(const char* msg)
{
    print_and_flush(FGRN SUCCESS_CHAR);
    print_and_flush(msg);
}

void clear_screen_server()
{
    print_header_server();
}

void clear_screen_client()
{
    print_header_client();
}

/// @brief Function that prints a loading spinner indefinitely
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

/// @brief Starts the thread that prints the loading spinner
void start_loading_spinner(pthread_t* tid)
{
#if PRETTY
    pthread_create(tid, NULL, loading_spinner, NULL);
#endif
}

/// @brief Stops the thread that prints the loading spinner
void stop_loading_spinner(pthread_t* tid)
{
#if PRETTY
    if (*tid != 0) {
        pthread_cancel(*tid);
        print_and_flush("\b \b");
        *tid = 0;
    }
#endif
}

// --------- MATH ---------

/// @brief Returns the number of digits of a number
/// @param n The number to count the digits of
/// @return The number of digits of the number
int digits(int n)
{
    if (n < 0)
        return digits((n == INT_MIN) ? INT_MAX : -n);
    if (n < 10)
        return 1;
    return 1 + digits(n / 10);
}

/// @brief Returns the maximum of two numbers
/// @param a The first number
/// @param b The second number
/// @return The maximum of the two numbers
int max(int a, int b)
{
    return a > b ? a : b;
}

/// @brief Returns the minimum of two numbers
/// @param a The first number
/// @param b The second number
/// @return The minimum of the two numbers
int min(int a, int b)
{
    return a < b ? a : b;
}

// --------- TERMINAL MANAGEMENT ---------

/// @brief Set the terminal input settings
/// @param policy The policy to set (with or without echo)
/// @return 0 if the operation was successful, -1 otherwise
int set_input(struct termios* policy)
{
#if PRETTY
    return tcsetattr(STDOUT_FILENO, TCSANOW, policy);
#else
    return 0;
#endif
}

/// @brief Ignore the input entered before the function call but not consumed
void ignore_previous_input()
{
    tcflush(STDIN_FILENO, TCIFLUSH);
}

/// @brief Initialize and get the terminal settings for the output
/// @param with_echo The termios struct to store the settings with echo
/// @param without_echo The termios struct to store the settings without echo
/// @return True if the operation was successful, false otherwise
bool init_output_settings(struct termios* with_echo, struct termios* without_echo)
{
#if PRETTY
    if (tcgetattr(STDOUT_FILENO, with_echo) != 0) {
        return false;
    }

    memcpy(without_echo, with_echo, sizeof(struct termios));
    without_echo->c_lflag &= ~ECHO;
#endif
    return true;
}

// --------- GAME MANAGEMENT ---------

/// @brief Initialize the game board with all zeros
/// @param matrix The matrix to initialize
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

/// @brief Initialize the array of pids with all zeros
/// @param pids_pointer The pointer to the array of pids
void init_pids(int* pids_pointer)
{
    // No need to use a semaphore, since only the server writes to this buffer at this point
    for (int i = 0; i < 3; i++) {
        pids_pointer[i] = 0;
    }
}

/// @brief set specified pid to the first empty slot (out of 3)
/// @param sem_id The semaphore id
/// @param game The game struct
/// @param pid The pid to set
/// @param username The username of the player
/// @param autoplay The autoplay mode
/// @return The index of the player in the game struct, or an error code
int record_join(int sem_id, tris_game_t* game, int pid, char* username, int autoplay)
{
    // Block all (catchable) signals in order to prevent deadlocks;
    // They will be re-enabled in init_signals() function in clients
    sigset_t mask;
    sigfillset(&mask);
    sigprocmask(SIG_BLOCK, &mask, NULL);

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
            strncpy(game->usernames[i], username, USERNAME_MAX_LEN);

            break;
        }
    }
    signal_semaphore(sem_id, PID_LOCK, 1);

    return player_index;
}

/// @brief Explicitly set the pid at the specified index
/// @param sem_id Semaphore id to lock pid array with
/// @param pids_pointer The pointer to the array of pids
/// @param index The index to set the pid at
/// @param pid The pid to set
void set_pid_at(int sem_id, int* pids_pointer, int index, int pid)
{
    wait_semaphore(sem_id, PID_LOCK, 1);
    pids_pointer[index] = pid;
    signal_semaphore(sem_id, PID_LOCK, 1);
}

/// @brief Set the pid at the specified index to 0
/// @param game The game struct
/// @param index The index to clear the pid at
void record_quit(tris_game_t* game, int index)
{
    int sem_id = get_semaphores(N_SEM);

    wait_semaphore(sem_id, PID_LOCK, 1);
    game->pids[index] = 0;
    // free(game->usernames[index]);
    signal_semaphore(sem_id, PID_LOCK, 1);
}

/// @brief Get the pid at the specified index
/// @param pids_pointer The pointer to the array of pids
/// @param index The index to get the pid from
/// @return The pid at the specified index
int get_pid_at(int* pids_pointer, int index)
{
    // no need to use semaphores here, as only the server read from this buffer,
    // and it does this only after clients have written to it and notified the server
    return pids_pointer[index];
}

/// @brief Check if the move is valid
/// @param matrix The matrix to check the move on
/// @param input The input string to check
/// @param move The move struct to store the row and column of the move
/// @return True if the move is valid, false otherwise
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

/// @brief Check if the game is ended
/// @param matrix The matrix to check the game on
/// @return The result of the game
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

/// @brief Minimax algorithm to find the best move
/// @param game_matrix The matrix to check the game on
/// @param depth The depth of the recursion
/// @param is_maximizing True if the current player is maximizing, false otherwise
/// @return The value of the best move
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

/// @brief Choose the best move for the AI with the minimax algorithm
/// @param game_matrix The matrix to check the game on
/// @param player_index The index of the player
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

/// @brief Choose a random move for the AI
/// @param game_matrix The matrix to check the game on
/// @param player_index The index of the player
void chooseRandomMove(int* game_matrix, int player_index)
{
    int random_index;

    do {
        random_index = rand() % (MATRIX_SIZE);
    } while (game_matrix[random_index] != 0);

    game_matrix[random_index] = player_index;
}

/// @brief Choose a random or the best move for the AI one after the other
/// @param game_matrix The matrix to check the game on
/// @param player_index The index of the player
/// @param cycles The number of moves currently made
void chooseRandomOrBestMove(int* game_matrix, int player_index, int cycles)
{
    if (cycles % 2 != 0)
        chooseBestMove(game_matrix, player_index);
    else
        chooseRandomMove(game_matrix, player_index);
}

/// @brief Choose the next move for the AI based on the difficulty
/// @param game_matrix The matrix to check the game on
/// @param difficulty The difficulty of the AI
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