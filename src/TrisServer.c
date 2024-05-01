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
void initSharedMemory();
void initSemaphores();
void setAtExitCleanup();
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

// Shared memory
int *matrix;
int matId;

pid_t *pids;
int pidId;

int *result;
int resId;

char *symbols;
int symId;

int *timeout;
int timId;

// Semaphores
int semId;

// Settings
char playerOneSymbol;
char playerTwoSymbol;

// State variables
bool firstCTRLCPressed = false;
bool started = false;
int turn = INITIAL_TURN;
int playersCount = 0;
int parsedTimeout;

// TODO: detect if another server is running and prevent the execution
// TODO: handle if IPCs with the same key are already present

int main(int argc, char *argv[])
{
    if (argc != N_ARGS_SERVER + 1)
    {
        errExit(USAGE_ERROR_SERVER);
    }

    // Initialization
    parseArgs(argc, argv);
    init();

    // Waiting for other players
    waitForPlayers();

    // If the server is here, it means that both players are connected...
    notifyOpponentReady();

    // ...and game can start
    started = true;
    printf(STARTS_PLAYER_MESSAGE, turn, pids[turn]);

    // Game loop
    while ((*result = isGameEnded(matrix)) == NOT_FINISHED)
    {
#if DEBUG
        printBoard(matrix, playerOneSymbol, playerTwoSymbol);
#endif
        // Wait for move
        waitForMove();

        // Notify the next player
        notifyNextMove();
    }

    // If the game is ended (i.e. no more in the loop), print the result
    switch (*result)
    {
    case 0:
        printf(DRAW_MESSAGE);
        break;
    case 1:
    case 2:
        printf(WINS_PLAYER_MESSAGE, *result, pids[*result]);
        break;
    }

    // Notify the player(s) still connected
    notifyGameEnded();
    exit(EXIT_SUCCESS);
}

void parseArgs(int argc, char *argv[])
{
    // Parsing timeout
    char *endptr;
    parsedTimeout = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0')
    {
        errExit(TIMEOUT_INVALID_CHAR_ERROR);
    }

    if (parsedTimeout < MINIMUM_TIMEOUT && parsedTimeout != 0)
    {
        errExit(TIMEOUT_TOO_LOW_ERROR);
    }

    // Parsing symbols
    if (strlen(argv[2]) != 1 || strlen(argv[3]) != 1)
    {
        errExit(SYMBOLS_LENGTH_ERROR);
    }

    playerOneSymbol = argv[2][0];
    playerTwoSymbol = argv[3][0];

    if (playerOneSymbol == playerTwoSymbol)
    {
        errExit(SYMBOLS_EQUAL_ERROR);
    }
}

void init()
{
    // Startup messages
    printWelcomeMessageServer();
    printLoadingMessage();

    // Data structures
    setAtExitCleanup();
    initSemaphores();
    initSharedMemory();
    initSignals();

    // Loading complete
    printLoadingCompleteMessage();
    printGameSettings();
}

void printGameSettings()
{
    printf(GAME_SETTINGS_MESSAGE, parsedTimeout, playerOneSymbol, playerTwoSymbol);
}

void initSharedMemory()
{
    matId = getAndInitSharedMemory(MATRIX_SIZE, MATRIX_ID);
    matrix = (int *)attachSharedMemory(matId);
    initBoard(matrix);

    pidId = getAndInitSharedMemory(PID_SIZE, PID_ID);
    pids = (pid_t *)attachSharedMemory(pidId);
    initPids(pids);
    setPidAt(pids, 0, getpid());

    resId = getAndInitSharedMemory(RESULT_SIZE, RESULT_ID);
    result = (int *)attachSharedMemory(resId);

    symId = getAndInitSharedMemory(SYMBOLS_SIZE, SYMBOLS_ID);
    symbols = (char *)attachSharedMemory(symId);
    symbols[0] = playerOneSymbol;
    symbols[1] = playerTwoSymbol;

    timId = getAndInitSharedMemory(sizeof(int), TIMEOUT_ID);
    timeout = (int *)attachSharedMemory(timId);
    *timeout = parsedTimeout;
}

void disposeMemory()
{
    disposeSharedMemory(matId);
    disposeSharedMemory(pidId);
    disposeSharedMemory(resId);
    disposeSharedMemory(symId);
    disposeSharedMemory(timId);
}

void initSemaphores()
{
    semId = getSemaphores(N_SEM);

    short unsigned values[N_SEM];
    values[WAIT_FOR_PLAYERS] = 0;
    values[WAIT_FOR_OPPONENT_READY] = 0;
    values[LOCK] = 1;
    values[PLAYER_ONE_TURN] = INITIAL_TURN == PLAYER_ONE ? 1 : 0;
    values[PLAYER_TWO_TURN] = INITIAL_TURN == PLAYER_TWO ? 1 : 0;
    values[WAIT_FOR_MOVE] = 0;

    setSemaphores(semId, N_SEM, values);
}

void disposeSemaphores()
{
    disposeSemaphore(semId);
}

void setAtExitCleanup()
{
    if (atexit(disposeSemaphores) || atexit(disposeMemory))
    {
        printError(INITIALIZATION_ERROR);
        exit(EXIT_FAILURE);
    }
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
        signal(SIGHUP, exitHandler) == SIG_ERR ||
        signal(SIGUSR1, playerQuitHandler) == SIG_ERR ||
        signal(SIGUSR2, playerQuitHandler) == SIG_ERR)
    {
        printError(INITIALIZATION_ERROR);
        exit(EXIT_FAILURE);
    }
}

void exitHandler(int sig)
{
    if (firstCTRLCPressed)
    {
        printf(CLOSING_MESSAGE);
        notifyServerQuit();
        exit(EXIT_SUCCESS);
    }
    firstCTRLCPressed = true;
    printf(CTRLC_AGAIN_TO_QUIT_MESSAGE);
}

void playerQuitHandler(int sig)
{
    int playerWhoQuitted = sig == SIGUSR1 ? PLAYER_ONE : PLAYER_TWO;
    int playerWhoStayed = playerWhoQuitted == PLAYER_ONE ? PLAYER_TWO : PLAYER_ONE;

    printf(A_PLAYER_QUIT_SERVER_MESSAGE, playerWhoQuitted, pids[playerWhoQuitted]);
    setPidAt(pids, playerWhoQuitted, 0);
    playersCount--;

    if (started)
    {
        *result = QUIT;
        printf(WINS_PLAYER_MESSAGE, playerWhoStayed, pids[playerWhoStayed]);
        notifyPlayerWhoWonForQuit(playerWhoStayed);
        exit(EXIT_SUCCESS);
    }
    else
    {
        printf(NEWLINE);
    }
}

void waitForPlayers()
{
    printAndFlush(WAITING_FOR_PLAYERS_MESSAGE);

    while (playersCount < 2)
    {
        do
        {
            errno = 0;
            waitSemaphore(semId, WAIT_FOR_PLAYERS, 1);
        } while (errno == EINTR);

        printAndFlush(NEWLINE);

        if (++playersCount == 1)
            printf(A_PLAYER_JOINED_SERVER_MESSAGE, pids[PLAYER_ONE], playerOneSymbol);
        else if (playersCount == 2)
            printf(ANOTHER_PLAYER_JOINED_SERVER_MESSAGE, pids[PLAYER_TWO], playerTwoSymbol);

        fflush(stdout);
    }
}

void notifyOpponentReady()
{
    signalSemaphore(semId, WAIT_FOR_OPPONENT_READY, 2);
}

void notifyPlayerWhoWonForQuit(int playerWhoWon)
{
    kill(pids[playerWhoWon], SIGUSR2);
}

void notifyGameEnded()
{
    kill(pids[PLAYER_ONE], SIGUSR2);
    kill(pids[PLAYER_TWO], SIGUSR2);
}

void notifyServerQuit()
{
    if (pids[PLAYER_ONE] != 0)
        kill(pids[PLAYER_ONE], SIGUSR1);

    if (pids[PLAYER_TWO] != 0)
        kill(pids[PLAYER_TWO], SIGUSR1);
}

void waitForMove()
{
    char *color = turn == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR;

    printf(WAITING_FOR_MOVE_SERVER_MESSAGE, color, turn, FNRM, pids[turn]);

    do
    {
        errno = 0;
        waitSemaphore(semId, WAIT_FOR_MOVE, 1);
    } while (errno == EINTR);
}

void notifyNextMove()
{
    char *color = turn == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR;

    printf(MOVE_RECEIVED_SERVER_MESSAGE, color, turn, FNRM, pids[turn]);

    turn = turn == 1 ? 2 : 1;
    signalSemaphore(semId, PLAYER_ONE_TURN + turn - 1, 1);
}