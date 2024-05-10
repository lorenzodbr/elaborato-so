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

void parse_args(int, char**);
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

    if (game->timeout < MINIMUM_TIMEOUT && game->timeout != 0) {
        errexit(TIMEOUT_TOO_LOW_ERROR);
    }
    // Parsing symbols
    if (strlen(argv[2]) != 1 || strlen(argv[3]) != 1) {
        errexit(SYMBOLS_LENGTH_ERROR);
    }

    game->symbols[0] = argv[2][0];
    game->symbols[1] = argv[3][0];

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

    // Dispose memory and semaphores at exit only if clients have already
    // quitted
    // if (atexit(waitOkToDispose))
    // {
    // errexit(INITIALIZATION_ERROR);
    // }

    init_signals();

    // Loading complete
    print_loading_complete_message();
}

void print_game_settings()
{
    print_and_flush(GAME_SETTINGS_MESSAGE);
    if (game->timeout == 0) {
        print_and_flush(INFINITE_TIMEOUT_SETTINGS_MESSAGE);
    } else {
        printf(TIMEOUT_SETTINGS_MESSAGE, game->timeout);
    }
    printf(PLAYER_ONE_SYMBOL_SETTINGS_MESSAGE, game->symbols[0]);
    printf(PLAYER_TWO_SYMBOL_SETTINGS_MESSAGE, game->symbols[1]);
}

void print_result()
{
    print_and_flush(FINAL_STATE_MESSAGE);
    print_board(game->matrix, game->symbols[0], game->symbols[1]);

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
    if ((output_customizable = init_output_settings(&with_echo, &without_echo))) {
        set_input(&without_echo);
        print_and_flush(HIDE_CARET);

        if (atexit(show_input)) {
            print_error(INITIALIZATION_ERROR);
            exit(EXIT_FAILURE);
        }
    }
}

void init_shared_memory()
{
    game_id = get_and_init_shared_memory(GAME_SIZE, GAME_ID);

    game = (tris_game_t*)attach_shared_memory(game_id);

    if (atexit(dispose_memory)) {
        errexit(INITIALIZATION_ERROR);
    }

    game->autoplay = NONE;
    init_board(game->matrix);
    init_pids(game->pids);
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
    if (sem_id != -1)
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
        print_error(INITIALIZATION_ERROR);
        exit(EXIT_FAILURE);
    }
}

void server_quit(int sig)
{
    printf(CLOSING_MESSAGE);
    notify_server_quit();
    exit(EXIT_SUCCESS);
}

void exit_handler(int sig)
{
    stop_loading_spinner(&spinner_tid);

    if (first_CTRLC_pressed) {
        server_quit(sig);
    }

    first_CTRLC_pressed = true;
    print_and_flush(CTRLC_AGAIN_TO_QUIT_MESSAGE);
}

void player_quit_handler(int sig)
{
    int playerWhoQuitted = sig == SIGUSR1 ? PLAYER_ONE : PLAYER_TWO;
    int playerWhoStayed = playerWhoQuitted == PLAYER_ONE ? PLAYER_TWO : PLAYER_ONE;
    char* playerWhoQuittedColor = playerWhoQuitted == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR;
    char* playerWhoStayedColor = playerWhoStayed == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR;

    printf(A_PLAYER_QUIT_SERVER_MESSAGE, playerWhoQuittedColor,
        playerWhoQuitted, game->usernames[playerWhoQuitted]);
    fflush(stdout);

#if DEBUG
    printf(WITH_PID_MESSAGE "\n", game->pids[playerWhoQuitted]);
#endif

    set_pid_at(sem_id, game->pids, playerWhoQuitted, 0);

    players_count--;

    if (started) {
        game->result = QUIT;
        printf("\n" WINS_PLAYER_MESSAGE, playerWhoStayedColor,
            playerWhoStayed, game->usernames[playerWhoStayed]);
        notify_player_who_won_for_quit(playerWhoStayed);
        exit(EXIT_SUCCESS);
    }
}

void wait_for_players()
{
    print_and_flush(WAITING_FOR_PLAYERS_MESSAGE);

    start_loading_spinner(&spinner_tid);

    int fork_ret = 0;

    while (players_count < 2) {
        if (game->autoplay != NONE && players_count == 1) {
            if ((fork_ret = fork()) == 0) {
                close(STDOUT_FILENO);
                execl("./bin/TrisClient", "./TrisClient", "AI", NULL);
                errexit(EXEC_ERROR);
            } else if (fork_ret < 0) {
                errexit(FORK_ERROR);
            } else {
                printf(AUTOPLAY_ENABLED_MESSAGE);

                if (game->autoplay == EASY)
                    printf(EASY_MODE_MESSAGE);
                else if (game->autoplay == MEDIUM)
                    printf(MEDIUM_MODE_MESSAGE);
                else
                    printf(HARD_MODE_MESSAGE);

                break;
            }
        }

        do {
            errno = 0;
            wait_semaphore(sem_id, WAIT_FOR_PLAYERS, 1);
        } while (errno == EINTR);

        stop_loading_spinner(&spinner_tid);
        print_and_flush(NEWLINE);

        if (++players_count == 1) {
            printf(A_PLAYER_JOINED_SERVER_MESSAGE,
                game->usernames[PLAYER_ONE]);

#if DEBUG
            printf(WITH_PID_MESSAGE, game->pids[PLAYER_ONE]);
#endif
        } else if (players_count == 2) {
            printf(ANOTHER_PLAYER_JOINED_SERVER_MESSAGE,
                game->usernames[PLAYER_TWO]);

#if DEBUG
            printf(WITH_PID_MESSAGE, game->pids[PLAYER_TWO]);
#endif
        }

        fflush(stdout);
    }

    print_and_flush(READY_TO_START_MESSAGE);
}

void notify_opponent_ready()
{
    signal_semaphore(sem_id, WAIT_FOR_OPPONENT_READY, 2);
}

void notify_player_who_won_for_quit(int player_who_won)
{
    kill(game->pids[player_who_won], SIGUSR2);
}

void notify_name_ended()
{
    kill(game->pids[PLAYER_ONE], SIGUSR2);
    kill(game->pids[PLAYER_TWO], SIGUSR2);
}

void notify_server_quit()
{
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

    do {
        errno = 0;
        wait_semaphore(sem_id, WAIT_FOR_MOVE, 1);
    } while (errno == EINTR);
}

void notify_next_move()
{
    printf(MOVE_RECEIVED_SERVER_MESSAGE,
        turn == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, turn,
        game->usernames[turn]);

    turn = turn == 1 ? 2 : 1;
    signal_semaphore(sem_id, PLAYER_ONE_TURN + turn - 1, 1);
}
