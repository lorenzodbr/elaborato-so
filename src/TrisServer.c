/************************************
 * VR487434 - Lorenzo Di Berardino
 * VR486588 - Filippo Milani
 * 09/05/2024
 *************************************/

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "utils/data.h"
#include "utils/globals.h"
#include "utils/shared_memory/shared_memory.h"

void parse_args(int, char*[]);
void init();
void init_terminal();
void init_shared_memory();
void init_semaphores();
void wait_for_players();
void dispose_memory();
void dispose_semaphores();
void init_signals();
void notify_opponent_ready();
void notify_player_who_won_for_quit(int);
void notify_name_ended();
void notify_server_quit();
void exit_handler(int);
void player_quit_handler(int);
void wait_for_move();
void notify_next_move();
void print_game_settings();
void server_quit(int);
void show_input();
void print_result();

// Shared memory
tris_game_t* game = NULL;
int game_id = -1;

// Semaphores
int sem_id = -1;

// State variables
bool first_CTRLC_pressed = false;
bool started = false;
int turn = INITIAL_TURN;
int players_count = 0;

// Terminal settings
struct termios with_echo, without_echo;
bool output_customizable = true;
pthread_t spinner_tid = 0;

// TODO: handle if IPCs with the same key are already present

int main(int argc, char* argv[])
{
    // Check arguments
    if (argc != N_ARGS_SERVER + 1) {
        errexit(USAGE_ERROR_SERVER);
    }

    // Initialization
    init();
    parse_args(argc, argv);

    // Show game settings
    print_game_settings();

    // Waiting for other players
    wait_for_players();

    // If the server is here, it means that both players are connected...
    notify_opponent_ready();

    // ...and game can start
    started = true;
    printf(STARTS_PLAYER_MESSAGE,
        INITIAL_TURN == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, turn,
        game->usernames[turn]);

    // Game loop
    while ((game->result = is_game_ended(game->matrix)) == NOT_FINISHED) {
#if DEBUG
        print_board(game->matrix, game->symbols[0], game->symbols[1]);
#else
        printf(NEWLINE);
#endif
        // Wait for move
        wait_for_move();

        // Notify the next player
        notify_next_move();
    }

    // If the game is ended (i.e. no more in the loop), print the result
    print_result();

    // Notify the player(s) still connected that the game is ended
    notify_name_ended();

    exit(EXIT_SUCCESS);
}

void parse_args(int argc, char* argv[])
{
    // Parsing timeout
    char* endptr;
    game->timeout = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        errexit(TIMEOUT_INVALID_CHAR_ERROR);
    }

    // Check if timeout is valid
    if (game->timeout < MINIMUM_TIMEOUT && game->timeout != 0) {
        errexit(TIMEOUT_TOO_LOW_ERROR);
    }
    // Parsing symbols
    if (strlen(argv[2]) != 1 || strlen(argv[3]) != 1) {
        errexit(SYMBOLS_LENGTH_ERROR);
    }

    game->symbols[0] = argv[2][0];
    game->symbols[1] = argv[3][0];

    // Check if symbols are the same
    if (game->symbols[0] == game->symbols[1]) {
        errexit(SYMBOLS_EQUAL_ERROR);
    }
}

void init()
{
    // Startup messages
    print_welcome_message_server();
    print_loading_message();

    // Check if another server is running
    if (are_there_attached_processes(game_id)) {
        errexit(SERVER_ALREADY_RUNNING_ERROR);
    }
    // Terminal settings
    init_terminal();

    // Data structures
    init_semaphores();
    init_shared_memory();

    // Signals
    init_signals();

    // Loading complete
    print_loading_complete_message();
}

void print_game_settings()
{
    print_and_flush(GAME_SETTINGS_MESSAGE);

    // Print timeout
    if (game->timeout == 0) {
        print_and_flush(INFINITE_TIMEOUT_SETTINGS_MESSAGE);
    } else {
        printf(TIMEOUT_SETTINGS_MESSAGE, game->timeout);
    }

    // Print symbols
    printf(PLAYER_ONE_SYMBOL_SETTINGS_MESSAGE, game->symbols[0]);
    printf(PLAYER_TWO_SYMBOL_SETTINGS_MESSAGE, game->symbols[1]);
}

void print_result()
{
    print_and_flush(FINAL_STATE_MESSAGE);
    print_board(game->matrix, game->symbols[0], game->symbols[1]);

    // Print the result based on the variable game->result
    switch (game->result) {
    case 0:
        printf(DRAW_MESSAGE);
        break;
    case 1:
    case 2:
        printf(WINS_PLAYER_MESSAGE,
            game->result == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR,
            game->result, game->usernames[game->result]);
        break;
    }
}

void init_terminal()
{
    // Check if the terminal is customizable
    if ((output_customizable = init_output_settings(&with_echo, &without_echo))) {
        // Hide the caret & output
        set_input(&without_echo);
        print_and_flush(HIDE_CARET);

        // Register the function to show the input at exit
        if (atexit(show_input)) {
            print_error(INITIALIZATION_ERROR);
            exit(EXIT_FAILURE);
        }
    }
}

void init_shared_memory()
{
    // Get shared memory
    game_id = get_and_init_shared_memory(GAME_SIZE, GAME_ID);

    game = (tris_game_t*)attach_shared_memory(game_id);

    if (atexit(dispose_memory)) {
        errexit(INITIALIZATION_ERROR);
    }

    // Initialize variables
    game->autoplay = NONE;
    init_board(game->matrix);
    init_pids(game->pids);

    // Register the server pid at the very first position
    set_pid_at(sem_id, game->pids, 0, getpid());
}

void dispose_memory()
{
    dispose_shared_memory(game_id);
    detach_shared_memory(game);
}

void init_semaphores()
{
    sem_id = get_semaphores(N_SEM);

    // Initialize semaphores
    short unsigned values[N_SEM];
    values[WAIT_FOR_PLAYERS] = 0;
    values[WAIT_FOR_OPPONENT_READY] = 0;
    values[PID_LOCK] = 1;
    values[PLAYER_ONE_TURN] = INITIAL_TURN == PLAYER_ONE ? 1 : 0;
    values[PLAYER_TWO_TURN] = INITIAL_TURN == PLAYER_TWO ? 1 : 0;
    values[WAIT_FOR_MOVE] = 0;

    set_semaphores(sem_id, N_SEM, values);

    if (atexit(dispose_semaphores)) {
        print_error(INITIALIZATION_ERROR);
        exit(EXIT_FAILURE);
    }
}

void dispose_semaphores()
{
    dispose_semaphore(sem_id);
}

void show_input()
{
    set_input(&with_echo);
    ignore_previous_input();
    print_and_flush(SHOW_CARET);

#if DEBUG
    printf(OUTPUT_RESTORED_SUCCESS);
#endif
}

void init_signals()
{
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGINT);
    sigdelset(&set, SIGTERM);
    sigdelset(&set, SIGHUP);
    sigdelset(&set, SIGUSR1);
    sigdelset(&set, SIGUSR2);
    sigprocmask(SIG_SETMASK, &set, NULL);

    if (signal(SIGINT, exit_handler) == SIG_ERR
        || signal(SIGTERM, exit_handler) == SIG_ERR
        || signal(SIGHUP, server_quit) == SIG_ERR
        || signal(SIGUSR1, player_quit_handler) == SIG_ERR
        || signal(SIGUSR2, player_quit_handler) == SIG_ERR) {
        errexit(INITIALIZATION_ERROR);
    }
}

void server_quit(int sig)
{
    // If the server quits, tell the clients (if still connected)
    printf(CLOSING_MESSAGE);
    notify_server_quit();
    exit(EXIT_SUCCESS);
}

void exit_handler(int sig)
{
    // Stop the spinner if it's still running
    stop_loading_spinner(&spinner_tid);

    if (first_CTRLC_pressed) {
        server_quit(sig);
    }

    first_CTRLC_pressed = true;
    print_and_flush(CTRLC_AGAIN_TO_QUIT_MESSAGE);
}

void player_quit_handler(int sig)
{

    int player_who_quitted = sig == SIGUSR1 ? PLAYER_ONE : PLAYER_TWO;
    int player_who_stayed = PLAYER_ONE + PLAYER_TWO - player_who_quitted;
    char* player_who_quitted_color = player_who_quitted == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR;
    char* player_who_stayed_color = player_who_stayed == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR;

    printf(A_PLAYER_QUIT_SERVER_MESSAGE, player_who_quitted_color,
        player_who_quitted, game->usernames[player_who_quitted]);
    fflush(stdout);

#if DEBUG
    printf(WITH_PID_MESSAGE "\n", game->pids[player_who_quitted]);
#endif

    // If a user quits, set its pid to 0
    set_pid_at(sem_id, game->pids, player_who_quitted, 0);

    // ...and decrease the number of players
    players_count--;

    // If the game has already started, this means that the player who quitted loses
    if (started) {
        game->result = QUIT;
        printf("\n" WINS_PLAYER_MESSAGE, player_who_stayed_color,
            player_who_stayed, game->usernames[player_who_stayed]);

        // Notify the player who stayed that he won
        notify_player_who_won_for_quit(player_who_stayed);
        exit(EXIT_SUCCESS);
    }
}

void wait_for_players()
{
    print_and_flush(WAITING_FOR_PLAYERS_MESSAGE);

    // Start the spinner
    start_loading_spinner(&spinner_tid);

    int fork_ret = 0;

    // Wait for players
    while (players_count < 2) {
        // If the server is in autoplay mode, start the client in another process
        if (game->autoplay != NONE && players_count == 1) {
            // Fork the process
            if ((fork_ret = fork()) == 0) {
                // Prevent the client from writing to the same stdout of the server
                close(STDOUT_FILENO);

                // Execute the client
                execl("./bin/TrisClient", "./TrisClient", "AI", NULL);

                // Executed only if execl fails
                errexit(EXEC_ERROR);
            } else if (fork_ret < 0) {
                // Executed only if fork fails
                errexit(FORK_ERROR);
            } else {
                // If the server is in autoplay mode, print the message
                printf(AUTOPLAY_ENABLED_MESSAGE);

                switch (game->autoplay) {
                case EASY:
                    printf(EASY_MODE_MESSAGE);
                    break;
                case MEDIUM:
                    printf(MEDIUM_MODE_MESSAGE);
                    break;
                case HARD:
                    printf(HARD_MODE_MESSAGE);
                    break;
                }

                break;
            }
        }

        // Wait for the first player
        do {
            errno = 0;
            wait_semaphore(sem_id, WAIT_FOR_PLAYERS, 1);
        } while (errno == EINTR);

        // Stop the spinner
        stop_loading_spinner(&spinner_tid);
        print_and_flush(NEWLINE);

        // If a player joined, but they are not two, print the message
        if (++players_count == 1) {
            printf(A_PLAYER_JOINED_SERVER_MESSAGE,
                game->usernames[PLAYER_ONE]);

#if DEBUG
            printf(WITH_PID_MESSAGE, game->pids[PLAYER_ONE]);
#endif
        } else if (players_count == 2) {
            // If the second player joined, print the message
            printf(ANOTHER_PLAYER_JOINED_SERVER_MESSAGE,
                game->usernames[PLAYER_TWO]);

#if DEBUG
            printf(WITH_PID_MESSAGE, game->pids[PLAYER_TWO]);
#endif
        }

        fflush(stdout);
    }

    // If the server is here, it means that both players are connected (or AI), so the game can start
    print_and_flush(READY_TO_START_MESSAGE);
}

void notify_opponent_ready()
{
    // Tell the first player that the second player is ready
    signal_semaphore(sem_id, WAIT_FOR_OPPONENT_READY, 2);
}

void notify_player_who_won_for_quit(int player_who_won)
{
    // Notify the player who won because the other player quitted
    kill(game->pids[player_who_won], SIGUSR2);
}

void notify_name_ended()
{
    // Notify both the players that the game is ended
    kill(game->pids[PLAYER_ONE], SIGUSR2);
    kill(game->pids[PLAYER_TWO], SIGUSR2);
}

void notify_server_quit()
{
    // Notify the remaining players that the server is quitting
    if (game->pids[PLAYER_ONE] != 0)
        kill(game->pids[PLAYER_ONE], SIGUSR1);

    if (game->pids[PLAYER_TWO] != 0)
        kill(game->pids[PLAYER_TWO], SIGUSR1);
}

void wait_for_move()
{
    printf(WAITING_FOR_MOVE_SERVER_MESSAGE,
        turn == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, turn,
        game->usernames[turn]);
    fflush(stdout);

    // Wait for the move of the current player
    do {
        errno = 0;
        wait_semaphore(sem_id, WAIT_FOR_MOVE, 1);
    } while (errno == EINTR);
}

void notify_next_move()
{
    // Notify the player who is waiting the opponent's move
    printf(MOVE_RECEIVED_SERVER_MESSAGE,
        turn == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, turn,
        game->usernames[turn]);

    turn = turn == 1 ? 2 : 1;
    signal_semaphore(sem_id, PLAYER_ONE_TURN + turn - 1, 1);
}
