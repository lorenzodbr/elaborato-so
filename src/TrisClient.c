/************************************
 * VR487434 - Lorenzo Di Berardino
 * VR486588 - Filippo Milani
 * 09/05/2024
 ************************************/

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "utils/data.h"
#include "utils/globals.h"
#include "utils/shared_memory/shared_memory.h"

void init();
void notify_player_ready();
void notify_move();
void init_shared_memory();
void init_semaphores();
void ask_for_input();
void wait_for_opponent();
void init_signals();
void exit_handler(int);
void quit_handler(int);
void check_results(int);
void server_quit_handler(int);
void wait_for_move();
void print_move_screen();
void init_timeout();
void reset_timeout();
void timeout_handler(int);
void show_input();
void init_terminal_settings();
void dispose_memory();

// Shared memory
tris_game_t* game = NULL;
int game_id = -1;

// Semaphores
int sem_id = -1;

// Signals
sigset_t set;

// State variables
bool first_CTRLC_pressed = false;
bool started = false;
bool active_player = false;

char* username = NULL;

int player_index = -1;
int cycles = 0;
int autoplay = NONE;

// Terminal settings
struct termios with_echo, without_echo;
bool output_customizable = true;
pthread_t spinner_tid = 0, timeout_tid = 0;

int main(int argc, char* argv[])
{
    // Check if the number of arguments is correct
    if (argc != N_ARGS_CLIENT + 1 && argc != N_ARGS_CLIENT) {
        errexit(USAGE_ERROR_CLIENT);
    }

    // if user wants to play against the AI,
    // check if the AI level is correct
    if (argc == N_ARGS_CLIENT + 1) {
        int check_easy = strcmp(argv[2], EASY_AI_CHAR);
        int check_medium = strcmp(argv[2], MEDIUM_AI_CHAR);
        int check_hard = strcmp(argv[2], HARD_AI_CHAR);

        if (check_easy != 0 && check_medium != 0 && check_hard != 0) {
            errexit(USAGE_ERROR_CLIENT);
        } else if (check_easy == 0) {
            autoplay = EASY;
        } else if (check_medium == 0) {
            autoplay = MEDIUM;
        } else {
            autoplay = HARD;
        }

        active_player = true;
    }

    // Check if the username length is correct
    username = argv[1];
    if (strlen(username) > USERNAME_MAX_LEN) {
        errexit(USERNAME_TOO_LONG_ERROR);
    }

    if (strlen(username) < USERNAME_MIN_LEN) {
        errexit(USERNAME_TOO_SHORT_ERROR);
    }

    // Prevent username duplication
    if (active_player && autoplay != NONE && strcmp(username, AI_USERNAME) == 0) {
        errexit(AI_USERNAME_ERROR);
    }

    // Initialize the client
    init();

    // After the initialization, the client is ready to play
    notify_player_ready();

    // Wait for the opponent to be ready
    wait_for_opponent();

    if (player_index != INITIAL_TURN) {
        print_move_screen();
        print_and_flush(OPPONENT_TURN_MESSAGE);
        print_and_flush(HIDE_CARET);
    }

    do {
        wait_for_move();

        print_and_flush(SHOW_CARET);

        // Prints before the move
        print_move_screen();

        if (autoplay == NONE || active_player)
            ask_for_input();
        else
            chooseNextMove(game->matrix, autoplay, player_index, cycles);

        stop_timeout_print(timeout_tid);

        notify_move();

        // Prints after the move
        print_move_screen();
        print_and_flush(OPPONENT_TURN_MESSAGE);

        set_input(&without_echo);
        print_and_flush(HIDE_CARET);

        stop_timeout_print(timeout_tid);
    } while (1);

    return EXIT_SUCCESS;
}

/// @brief Initializes the client
void init()
{
    // Print the welcome message
    print_welcome_message_client(username);
    print_loading_message();

    // Initialize IPCs and terminal settings
    init_semaphores();
    init_shared_memory();
    init_signals();
    init_terminal_settings();

    // Set the seed for the random number generator (used by the AI)
    srand(time(NULL));

    // Print the loading complete message
    print_loading_complete_message();
}

/// @brief Initializes the shared memory
void init_shared_memory()
{
    // Get the shared memory without creating it
    game_id = get_shared_memory(GAME_SIZE, GAME_ID);

    // If the shared memory getter returns -1, then no server is running
    if (game_id < 0) {
        errexit(NO_SERVER_FOUND_ERROR);
    }

    // Otherwise, attach the shared memory
    game = (tris_game_t*)attach_shared_memory(game_id);

#if DEBUG
    printf(SHARED_MEMORY_OBTAINED_SUCCESS, game_id);
#endif

    // Join the game by setting the player PID and username
    if ((player_index = record_join(sem_id, game, getpid(), username, autoplay)) == TOO_MANY_PLAYERS_ERROR_CODE)
        errexit(TOO_MANY_PLAYERS_ERROR);
    else if (player_index == SAME_USERNAME_ERROR_CODE)
        errexit(SAME_USERNAME_ERROR);
    else if (player_index == AUTOPLAY_NOT_ALLOWED_ERROR_CODE)
        errexit(AUTOPLAY_NOT_ALLOWED_ERROR);

    // Set the local autoplay flag
    autoplay = game->autoplay;

#if DEBUG
    printf(SERVER_FOUND_SUCCESS, game->pids[SERVER]);
#endif

    // Register the dispose_memory function to be called at exit
    if (atexit(dispose_memory))
        errexit(INITIALIZATION_ERROR);
}

/// @brief Disposes the shared memory
void dispose_memory()
{
    detach_shared_memory(game);
}

/// @brief Initializes the semaphores
void init_semaphores()
{
    // Get the semaphores
    sem_id = get_semaphores(N_SEM);
}

/// @brief Initializes the signals
void init_signals()
{
    // Set the signals to handle
    sigfillset(&set);
    sigdelset(&set, SIGINT);
    sigdelset(&set, SIGUSR1);
    sigdelset(&set, SIGUSR2);
    sigdelset(&set, SIGHUP);
    sigdelset(&set, SIGTERM);
    sigdelset(&set, SIGALRM);
    sigprocmask(SIG_SETMASK, &set, NULL);

    // Register the signal handlers
    if (signal(SIGINT, exit_handler) == SIG_ERR
        || signal(SIGUSR1, server_quit_handler) == SIG_ERR
        || signal(SIGUSR2, check_results) == SIG_ERR
        || signal(SIGHUP, quit_handler) == SIG_ERR
        || signal(SIGTERM, exit_handler) == SIG_ERR)
        errexit(INITIALIZATION_ERROR);
}

/// @brief Initializes the terminal settings
void init_terminal_settings()
{
#if PRETTY
    // Initialize the terminal settings: if possible, set the terminal to raw mode
    if ((output_customizable = init_output_settings(&with_echo, &without_echo))) {
        set_input(&without_echo);

        // Register the show_input function to be called at exit
        if (atexit(show_input)) {
            print_error(INITIALIZATION_ERROR);
            exit(EXIT_FAILURE);
        }
    }
#endif
}

/// @brief Sets the terminal to show the input
void show_input()
{
    // Restore the terminal settings
#if PRETTY
    set_input(&with_echo);
#endif

    print_and_flush(SHOW_CARET);

#if DEBUG && PRETTY
    printf(OUTPUT_RESTORED_SUCCESS);
#endif
}

/// @brief Notifies the server that the player is ready
void notify_player_ready()
{
    // Tell the server a user arrived
    signal_semaphore(sem_id, WAIT_FOR_PLAYERS, 1);
}

/// @brief Notifies the server that the player made a move
void notify_move()
{
    // Tell the server a user made a move
    cycles++;
    signal_semaphore(sem_id, WAIT_FOR_MOVE, 1);
}

/// @brief Waits for the opponent to be ready
void wait_for_opponent()
{
    // If the game is in autoplay mode, the opponent "always" is ready
    if (autoplay != NONE)
        return;

    // Show the waiting message
    print_and_flush(game->usernames[player_index - 1]);
    print_and_flush(WAITING_FOR_OPPONENT_MESSAGE);
    print_and_flush(HIDE_CARET);
    start_loading_spinner(&spinner_tid);

    // Wait for the opponent to be ready. If stopped by signal, retry
    do {
        errno = 0;
        wait_semaphore(sem_id, WAIT_FOR_OPPONENT_READY, 1);

        // If the semaphore is removed, it means the server quitted.
        // Then, you just need to wait until the signal sent by the server
        // is catched by the signal handler
        if (errno == EIDRM)
            sigsuspend(&set);
    } while (errno == EINTR);

    // When the opponent is ready, stop the spinner and print the message
    stop_loading_spinner(&spinner_tid);
    print_and_flush(OPPONENT_READY_MESSAGE);
    print_and_flush(SHOW_CARET);
    started = true;
}

/// @brief Waits for the opponent to make a move
void wait_for_move()
{
    // Wait for the opponent to make a move
    do {
        errno = 0;
        wait_semaphore(sem_id, PLAYER_ONE_TURN + player_index - 1, 1);

        // If the semaphore is removed, it means the server quitted.
        // Then, you just need to wait until the signal sent by the server
        // is catched by the signal handler
        if (errno == EIDRM) {
            sigsuspend(&set);
        }
    } while (errno == EINTR);

    // Flush the input buffer to prevent buffer overflow
    ignore_previous_input();
}

/// @brief Handles the timeout expiration
void timeout_handler(int sig)
{
    // If the timeout expires, the player loses. This is achieved by simulating a quit signal
    print_move_screen();
    printf(TIMEOUT_LOSS_MESSAGE);
    quit_handler(sig);
    exit(EXIT_FAILURE);
}

/// @brief Initializes the timeout based on the game settings
void init_timeout()
{
    // If the timeout is set to 0, do nothing
    if (game->timeout == 0) {
        return;
    }

    // Set the timeout handler
    alarm(game->timeout);
    signal(SIGALRM, timeout_handler);

    // Start the timeout print
    start_timeout_print(&timeout_tid, &game->timeout);
}

/// @brief Stops the timeout
void reset_timeout()
{
    // If the timeout is set to 0, do nothing
    if (game->timeout == 0) {
        return;
    }

    // Reset the timeout
    alarm(0);
}

/// @brief Asks the player for a move
void ask_for_input()
{
    char input[MOVE_INPUT_LEN + 1];

#if PRETTY
    // Restore the terminal settings
    show_input(without_echo);

    // Print the input message shifting it to the right by the length of the timeout
    print_spaces((digits(game->timeout) + 3) * (game->timeout != 0));
#endif

    printf(INPUT_A_MOVE_MESSAGE);

    // Initialize the timeout
    init_timeout();

    // Read the move (only the first characters are considered)
    if(scanf("%" STR(MOVE_INPUT_LEN) "s", input) == EOF){
        quit_handler(0);
        PUNISH_USER;
        print_and_flush(NEWLINE);
        errexit(EOF_ERROR);
    }
    while (getchar() != '\n') // needed to prevent buffer overflow
        ;

    move_t move;

    // Check if the move is valid
    while (!is_valid_move(game->matrix, input, &move)) {
        first_CTRLC_pressed = false; // Reset firstCTRLCPressed if something else is inserted

        PUNISH_USER; // Eject the CD tray to annoy the user

        // Print the error message and ask for a new move
#if PRETTY
        print_spaces((digits(game->timeout) + 3) * (game->timeout != 0));
#endif
        print_error(INVALID_MOVE_ERROR);
        scanf("%s", input);
    }

    // Update the matrix with the new move
    game->matrix[move.row + move.col * MATRIX_SIDE_LEN] = player_index;

    // Reset the timeout after move is made
    reset_timeout();
}

/// @brief Prints the board before and after a move
void print_move_screen()
{
    clear_screen_client();

    // Print the game symbols
    print_symbol(game->symbols[player_index - 1], player_index, username);

    // Print the timeout
    print_timeout(game->timeout);

    // Print the board
    print_board(game->matrix, game->symbols[0], game->symbols[1]);
}

/// @brief Checks the results of the game
void check_results(int sig)
{
    print_move_screen();

    // Check the result of the game
    if (game->result == DRAW) {
        print_and_flush(DRAW_MESSAGE);
    } else if (game->result == QUIT) {
        print_and_flush(YOU_WON_FOR_QUIT_MESSAGE);
    } else if (game->result != player_index) {
        print_and_flush(YOU_LOST_MESSAGE);
    } else {
        print_and_flush(YOU_WON_MESSAGE);
    }

    exit(EXIT_SUCCESS);
}

/// @brief Handles the player controlled exit
void exit_handler(int sig)
{
    if (!started)
        stop_loading_spinner(&spinner_tid);

    // If the user presses CTRL+C twice, the client exits
    if (first_CTRLC_pressed) {
        if (active_player || autoplay == NONE)
            kill(game->pids[SERVER], player_index == 1 ? SIGUSR1 : SIGUSR2);

        print_and_flush(CLOSING_MESSAGE);
        exit(EXIT_SUCCESS);
    }

    first_CTRLC_pressed = true;
    printf(CTRLC_AGAIN_TO_QUIT_MESSAGE);

    if (started) {
        printf(THIS_WAY_YOU_WILL_LOSE_MESSAGE);
    }

    fflush(stdout);
}

/// @brief Handles the player sudden quit
void quit_handler(int sig)
{
    // Tell the server that the user quit
    if (active_player || autoplay == NONE)
        kill(game->pids[SERVER], player_index == 1 ? SIGUSR1 : SIGUSR2);
}

/// @brief Handles the server quit
void server_quit_handler(int sig)
{
    // If the server quits, the client displays a message and exits
    stop_loading_spinner(&spinner_tid);
    print_and_flush(SERVER_QUIT_MESSAGE);
    exit(EXIT_FAILURE);
}