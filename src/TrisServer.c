#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

#include "utils/data.h"
#include "utils/globals.h"
#include "utils/shared_memory/shared_memory.h"

void parseArgs(int argc, char *argv[]);
void init();
void initTerminal();
void initSharedMemory();
void initSemaphores();
void waitForPlayers();
void disposeMemory();
void disposeSemaphores();
void initSignals();
void notifyOpponentReady();
void notifyPlayerWhoWonForQuit(int playerWhoWon);
void notifyGameEnded();
void notifyServerQuit();
void exitHandler(int sig);
void playerQuitHandler(int sig);
void waitForMove();
void notifyNextMove();
void printGameSettings();
void serverQuit(int sig);
void showInput();
void printResult();
void waitOkToDispose();

// Shared memory
tris_game_t *game = NULL;
int gameId = -1;

// Semaphores
int semId = -1;

// State variables
bool firstCTRLCPressed = false;
bool started = false;
int turn = INITIAL_TURN;
int playersCount = 0;

// Terminal settings
struct termios withEcho, withoutEcho;
bool outputCustomizable = true;
pthread_t *spinnerTid = NULL;

// TODO: handle if IPCs with the same key are already present

int main(int argc, char *argv[])
{
    if (argc != N_ARGS_SERVER + 1)
    {
        errExit(USAGE_ERROR_SERVER);
    }

    // Initialization
    init();
    parseArgs(argc, argv);

    // Show game settings
    printGameSettings();

    // Waiting for other players
    waitForPlayers();

    // If the server is here, it means that both players are connected...
    notifyOpponentReady();

    // ...and game can start
    started = true;
    printf(STARTS_PLAYER_MESSAGE, INITIAL_TURN == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, turn, game->usernames[turn]);

    // Game loop
    while ((game->result = isGameEnded(game->matrix)) == NOT_FINISHED)
    {
#if DEBUG
        printBoard(game->matrix, game->symbols[0], game->symbols[1]);
#else
        printf(NEWLINE);
#endif
        // Wait for move
        waitForMove();

        // Notify the next player
        notifyNextMove();
    }

    // If the game is ended (i.e. no more in the loop), print the result
    printResult();

    // Notify the player(s) still connected that the game is ended
    notifyGameEnded();

    exit(EXIT_SUCCESS);
}

void parseArgs(int argc, char *argv[])
{
    // Parsing timeout
    char *endptr;
    game->timeout = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0')
    {
        errExit(TIMEOUT_INVALID_CHAR_ERROR);
    }

    if (game->timeout < MINIMUM_TIMEOUT && game->timeout != 0)
    {
        errExit(TIMEOUT_TOO_LOW_ERROR);
    }

    // Parsing symbols
    if (strlen(argv[2]) != 1 || strlen(argv[3]) != 1)
    {
        errExit(SYMBOLS_LENGTH_ERROR);
    }

    game->symbols[0] = argv[2][0];
    game->symbols[1] = argv[3][0];

    if (game->symbols[0] == game->symbols[1])
    {
        errExit(SYMBOLS_EQUAL_ERROR);
    }
}

void init()
{
    // Startup messages
    printWelcomeMessageServer();
    printLoadingMessage();

    // Check if another server is running
    if (areThereAttachedProcesses(gameId))
    {
        errExit(SERVER_ALREADY_RUNNING_ERROR);
    }

    // Terminal settings
    initTerminal();

    // Data structures
    initSemaphores();
    initSharedMemory();

    // Dispose memory and semaphores at exit only if clients have already quitted
    // if (atexit(waitOkToDispose))
    // {
    //     errExit(INITIALIZATION_ERROR);
    // }

    initSignals();

    // Loading complete
    printLoadingCompleteMessage();
}

void printGameSettings()
{
    printAndFlush(GAME_SETTINGS_MESSAGE);
    if (game->timeout == 0)
    {
        printAndFlush(INFINITE_TIMEOUT_SETTINGS_MESSAGE);
    }
    else
    {
        printf(TIMEOUT_SETTINGS_MESSAGE, game->timeout);
    }
    printf(PLAYER_ONE_SYMBOL_SETTINGS_MESSAGE, game->symbols[0]);
    printf(PLAYER_TWO_SYMBOL_SETTINGS_MESSAGE, game->symbols[1]);
}

void printResult()
{
    printAndFlush(FINAL_STATE_MESSAGE);
    printBoard(game->matrix, game->symbols[0], game->symbols[1]);

    switch (game->result)
    {
    case 0:
        printf(DRAW_MESSAGE);
        break;
    case 1:
    case 2:
        printf(WINS_PLAYER_MESSAGE, game->result == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR,
               game->result, game->usernames[game->result]);
        break;
    }
}

void initTerminal()
{
    if ((outputCustomizable = initOutputSettings(&withEcho, &withoutEcho)))
    {
        setInput(&withoutEcho);

        if (atexit(showInput))
        {
            printError(INITIALIZATION_ERROR);
            exit(EXIT_FAILURE);
        }
    }
}

void initSharedMemory()
{
    gameId = getAndInitSharedMemory(GAME_SIZE, GAME_ID);

    game = (tris_game_t *)attachSharedMemory(gameId);

    spinnerTid = malloc(sizeof(pthread_t));

    if (atexit(disposeMemory) || spinnerTid == NULL)
    {
        errExit(INITIALIZATION_ERROR);
    }

    initBoard(game->matrix);
    initPids(game->pids);
    setPidAt(game->pids, 0, getpid());
}

void waitOkToDispose()
{
    if(!started && playersCount == 0){
        return;
    }

    do
    {
        errno = 0;
        waitSemaphore(semId, OK_TO_DISPOSE, started ? 2 : playersCount);
    } while (errno == EINTR);
}

void disposeMemory()
{
    disposeSharedMemory(gameId);
    detachSharedMemory(game);
    free(spinnerTid);
}

void initSemaphores()
{
    semId = getSemaphores(N_SEM);

    short unsigned values[N_SEM];
    values[WAIT_FOR_PLAYERS] = 0;
    values[WAIT_FOR_OPPONENT_READY] = 0;
    values[PID_LOCK] = 1;
    values[PLAYER_ONE_TURN] = INITIAL_TURN == PLAYER_ONE ? 1 : 0;
    values[PLAYER_TWO_TURN] = INITIAL_TURN == PLAYER_TWO ? 1 : 0;
    values[WAIT_FOR_MOVE] = 0;
    values[OK_TO_DISPOSE] = 0;

    setSemaphores(semId, N_SEM, values);

    if (atexit(disposeSemaphores))
    {
        printError(INITIALIZATION_ERROR);
        exit(EXIT_FAILURE);
    }
}

void disposeSemaphores()
{
    if (semId != -1)
        disposeSemaphore(semId);
}

void showInput()
{
    setInput(&withEcho);
    ignorePreviousInput();

#if DEBUG
    printf(OUTPUT_RESTORED_SUCCESS);
#endif
}

void initSignals()
{
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGINT);
    sigdelset(&set, SIGTERM);
    sigdelset(&set, SIGHUP);
    sigdelset(&set, SIGUSR1);
    sigdelset(&set, SIGUSR2);
    sigprocmask(SIG_SETMASK, &set, NULL);

    if (signal(SIGINT, exitHandler) == SIG_ERR ||
        signal(SIGTERM, exitHandler) == SIG_ERR ||
        signal(SIGHUP, serverQuit) == SIG_ERR ||
        signal(SIGUSR1, playerQuitHandler) == SIG_ERR ||
        signal(SIGUSR2, playerQuitHandler) == SIG_ERR)
    {
        printError(INITIALIZATION_ERROR);
        exit(EXIT_FAILURE);
    }
}

void serverQuit(int sig)
{
    printf(CLOSING_MESSAGE);
    notifyServerQuit();
    exit(EXIT_SUCCESS);
}

void exitHandler(int sig)
{
    stopLoadingSpinner(&spinnerTid);

    if (firstCTRLCPressed)
    {
        serverQuit(sig);
    }

    firstCTRLCPressed = true;
    printAndFlush(CTRLC_AGAIN_TO_QUIT_MESSAGE);
}

void playerQuitHandler(int sig)
{
    int playerWhoQuitted = sig == SIGUSR1 ? PLAYER_ONE : PLAYER_TWO;
    int playerWhoStayed = playerWhoQuitted == PLAYER_ONE ? PLAYER_TWO : PLAYER_ONE;
    char *playerWhoQuittedColor = playerWhoQuitted == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR;
    char *playerWhoStayedColor = playerWhoStayed == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR;

    printf(A_PLAYER_QUIT_SERVER_MESSAGE, playerWhoQuittedColor, playerWhoQuitted, game->usernames[playerWhoQuitted]);
    fflush(stdout);

#if DEBUG
    printf(WITH_PID_MESSAGE "\n", game->pids[playerWhoQuitted]);
#endif

    setPidAt(game->pids, playerWhoQuitted, 0);

    playersCount--;

    if (started)
    {
        game->result = QUIT;
        printf("\n" WINS_PLAYER_MESSAGE, playerWhoStayedColor, playerWhoStayed, game->usernames[playerWhoStayed]);
        notifyPlayerWhoWonForQuit(playerWhoStayed);
        exit(EXIT_SUCCESS);
    }
}

void waitForPlayers()
{
    printAndFlush(WAITING_FOR_PLAYERS_MESSAGE);

    startLoadingSpinner(&spinnerTid);

    while (playersCount < 2)
    {
        do
        {
            errno = 0;
            waitSemaphore(semId, WAIT_FOR_PLAYERS, 1);
        } while (errno == EINTR);

        stopLoadingSpinner(&spinnerTid);
        printAndFlush(NEWLINE);

        if (++playersCount == 1)
        {
            printf(A_PLAYER_JOINED_SERVER_MESSAGE, game->usernames[PLAYER_ONE]);

#if DEBUG
            printf(WITH_PID_MESSAGE, game->pids[PLAYER_ONE]);
#endif
        }
        else if (playersCount == 2)
        {
            printf(ANOTHER_PLAYER_JOINED_SERVER_MESSAGE, game->usernames[PLAYER_TWO]);

#if DEBUG
            printf(WITH_PID_MESSAGE, game->pids[PLAYER_TWO]);
#endif
        }

        fflush(stdout);
    }

    printAndFlush(READY_TO_START_MESSAGE);
}

void notifyOpponentReady()
{
    signalSemaphore(semId, WAIT_FOR_OPPONENT_READY, 2);
}

void notifyPlayerWhoWonForQuit(int playerWhoWon)
{
    kill(game->pids[playerWhoWon], SIGUSR2);
}

void notifyGameEnded()
{
    kill(game->pids[PLAYER_ONE], SIGUSR2);
    kill(game->pids[PLAYER_TWO], SIGUSR2);
}

void notifyServerQuit()
{
    if (game->pids[PLAYER_ONE] != 0)
        kill(game->pids[PLAYER_ONE], SIGUSR1);

    if (game->pids[PLAYER_TWO] != 0)
        kill(game->pids[PLAYER_TWO], SIGUSR1);
}

void waitForMove()
{
    printf(WAITING_FOR_MOVE_SERVER_MESSAGE,
           turn == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR,
           turn,
           game->usernames[turn]);
    fflush(stdout);

    do
    {
        errno = 0;
        waitSemaphore(semId, WAIT_FOR_MOVE, 1);
    } while (errno == EINTR);
}

void notifyNextMove()
{
    printf(MOVE_RECEIVED_SERVER_MESSAGE,
           turn == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR,
           turn,
           game->usernames[turn]);

    turn = turn == 1 ? 2 : 1;
    signalSemaphore(semId, PLAYER_ONE_TURN + turn - 1, 1);
}