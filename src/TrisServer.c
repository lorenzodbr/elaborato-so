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
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>

#include "utils/data.h"
#include "utils/globals.h"
#include "utils/shared_memory/shared_memory.h"
#include "utils/semaphores/semaphores.h"

void parse_args(int, char*[]);
void init();
void init_terminal();
void init_shared_memory();
void init_semaphores();
void init_signals();
void wait_for_players();
void dispose_memory();
void dispose_semaphores();
void notify_opponent_ready();
void notify_player_who_won_for_quit(int);
void notify_name_ended();
void notify_server_quit();
void exit_handler(int);
void player_quit_handler(int);
void wait_for_move();
void notify_next_move();
void print_game_settings();
void quit_handler(int);
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
        printf(USAGE_ERROR_SERVER, argv[0]);
        exit(EXIT_FAILURE);
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

/// @brief Parse the arguments passed to the server
/// @param argc The number of arguments
/// @param argv The arguments
void parse_args(int argc, char* argv[])
{
    // Parsing timeout
    char* str_ptr;
    game->timeout = strtol(argv[1], &str_ptr, 10);
    if (*str_ptr != '\0') {
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

// ------------------ INITIALIZERS -------------------

/// @brief Initialize the server
void init()
{
    // Startup messages
    print_welcome_message_server();
    print_loading_message();

    // Check if another server is running
    if (is_another_server_running(&game_id, &game)) {
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

/// @brief Initialize the terminal settings
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

/// @brief Initialize the shared memory
void init_shared_memory()
{
    if (game_id == -1 || game == NULL) {
        // Get shared memory
        game_id = get_and_init_shared_memory(GAME_SIZE, GAME_ID);
        game = (tris_game_t*)attach_shared_memory(game_id);
    }
#if DEBUG
    else {
        printf(FOUND_CRASHED_SERVER_MESSAGE);
    }
#endif

    if (atexit(dispose_memory)) {
        errexit(INITIALIZATION_ERROR);
    }

    // Initialize variables
    game->result = NOT_FINISHED;
    game->autoplay = NONE;
    init_board(game->matrix);
    init_pids(game->pids);
    memset(game->client_path, 0, sizeof(game->client_path));

    // Register the server pid at the very first position
    set_pid_at(sem_id, game->pids, 0, getpid());
}

/// @brief Dispose the shared memory
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

/// @brief Initialize the signals
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
        || signal(SIGHUP, quit_handler) == SIG_ERR
        || signal(SIGUSR1, player_quit_handler) == SIG_ERR
        || signal(SIGUSR2, player_quit_handler) == SIG_ERR) {
        errexit(INITIALIZATION_ERROR);
    }
}

// ------------------ DISPOSERS -------------------

/// @brief Dispose the shared memory
void dispose_memory()
{
    dispose_shared_memory(game_id);
    detach_shared_memory(game);
}

void dispose_semaphores()
{
    dispose_semaphore(sem_id);
}

// --------------- OUTPUT SETTINGS ----------------

/// @brief Show input in the terminal
void show_input()
{
    set_input(&with_echo);
    ignore_previous_input();
    print_and_flush(SHOW_CARET);

#if DEBUG && PRETTY
    printf(OUTPUT_RESTORED_SUCCESS);
#endif
}

// ----------------- PRINT FUNCTIONS -----------------

/// @brief Print the game settings specified by the user
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

/// @brief Print the result of the game
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

// ----------------- SIGNAL HANDLERS -----------------

/// @brief Handle the quit
void quit_handler(int sig)
{
    // If the server quits, tell the clients (if still connected)
    print_and_flush(CLOSING_MESSAGE);
    notify_server_quit();
    exit(EXIT_SUCCESS);
}

/// @brief Handle the exit signal
void exit_handler(int sig)
{
    // Stop the spinner if it's still running
    stop_loading_spinner(&spinner_tid);

    if (first_CTRLC_pressed) {
        quit_handler(sig);
    }

    first_CTRLC_pressed = true;
    print_and_flush(CTRLC_AGAIN_TO_QUIT_MESSAGE);
}

/// @brief Handle the player quit
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
        if(game->autoplay != NONE && player_who_quitted == PLAYER_TWO) {
            notify_server_quit();
            exit(EXIT_FAILURE);
        }

        game->result = QUIT;
        printf("\n" WINS_PLAYER_MESSAGE, player_who_stayed_color,
            player_who_stayed, game->usernames[player_who_stayed]);

        // Notify the player who stayed that he won
        notify_player_who_won_for_quit(player_who_stayed);
        exit(EXIT_SUCCESS);
    }
}

// ----------------- GAME FUNCTIONS -----------------

/// @brief Wait for the players to join
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

                char* client_path;

                // Check if client set its path correctly;
                // If not, try to use the default path
                if(game->client_path[0] == '\0') {
                    client_path = "./bin/TrisClient";
                } else {
                    client_path = game->client_path;
                }

                // Execute the client
                execl(client_path, CLIENT_EXEC_NAME, AI_USERNAME, NULL);

                // Executed only if execl fails
                exit(EXIT_FAILURE);
            } else if (fork_ret < 0) {
                // Executed only if fork fails
                // Prevent connected client from hanging
                notify_server_quit();
                errexit(FORK_ERROR);
            } else {
                do {
                    errno = 0;
                    wait_semaphore(sem_id, WAIT_FOR_PLAYERS, 1);
                } while (errno == EINTR);

                // Check if the AI client got executed or failed
                if(get_pid_at(game->pids, PLAYER_TWO) == 0) {
                    notify_server_quit();
                    errexit(EXEC_ERROR);
                }

                // If the server is in autoplay mode, print the message
                printf(AUTOPLAY_ENABLED_MESSAGE);

                switch (game->autoplay) {
                case EASY:
                    printf(EASY_MODE_MESSAGE);
                    break;
                case MEDIUM:
                    printf(MEDIUM_MODE_MESSAGE);
                    break;
                case IMPOSSIBLE:
                    printf(IMPOSSIBLE_MODE_MESSAGE);
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

/// @brief Tell the opponent that the player is ready
void notify_opponent_ready()
{
    signal_semaphore(sem_id, WAIT_FOR_OPPONENT_READY, 2);
}

/// @brief Notify the player who won because the other player quitted
void notify_player_who_won_for_quit(int player_who_won)
{
    kill(game->pids[player_who_won], SIGUSR2);
}

/// @brief Notify the players that the game is ended
void notify_name_ended()
{
    kill(game->pids[PLAYER_ONE], SIGUSR2);
    kill(game->pids[PLAYER_TWO], SIGUSR2);
}

/// @brief Notify that the server is quitting
void notify_server_quit()
{
    if (game->pids[PLAYER_ONE] != 0)
        kill(game->pids[PLAYER_ONE], SIGUSR1);

    if (game->pids[PLAYER_TWO] != 0)
        kill(game->pids[PLAYER_TWO], SIGUSR1);
}

/// @brief Wait for the move of the current player
void wait_for_move()
{
    printf(WAITING_FOR_MOVE_SERVER_MESSAGE,
        turn == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, turn,
        game->usernames[turn]);
    fflush(stdout);

    do {
        errno = 0;
        wait_semaphore(sem_id, WAIT_FOR_MOVE, 1);
    } while (errno == EINTR);
}

/// @brief Notify the next player that it's their turn
void notify_next_move()
{
    printf(MOVE_RECEIVED_SERVER_MESSAGE,
        turn == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, turn,
        game->usernames[turn]);

    turn = turn == 1 ? 2 : 1;
    signal_semaphore(sem_id, PLAYER_ONE_TURN + turn - 1, 1);
}
