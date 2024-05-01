#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>

#include "utils/data.h"
#include "utils/globals.h"
#include "utils/shared_memory/shared_memory.h"

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

// Shared memory
int *matrix;
int matId;

pid_t *pids;
int pidId;

int *result;
int resId;

char *symbols;
int symId;

// Semaphores
int semId;

// Settings
int timeout;
char playerOneSymbol;
char playerTwoSymbol;

// State variables
bool firstCTRLCPressed = false;
bool started = false;
int turn = INITIAL_TURN;
int playersCount = 0;

int main(int argc, char *argv[])
{
    if (argc != N_ARGS_SERVER + 1)
    {
        errExit(USAGE_ERROR_SERVER);
    }

    timeout = atoi(argv[1]);
    playerOneSymbol = argv[2][0];
    playerTwoSymbol = argv[3][0];

    if (strlen(argv[2]) != 1 || strlen(argv[3]) != 1)
    {
        printError(SYMBOLS_LENGTH_ERROR);
        exit(EXIT_FAILURE);
    }

    init();
    waitForPlayers();
    notifyOpponentReady();

    started = true;

    printf(STARTS_PLAYER_MESSAGE, turn, pids[turn]);

    while ((*result = isGameEnded(matrix)) == NOT_FINISHED)
    {
#if DEBUG
        printBoard(matrix, playerOneSymbol, playerTwoSymbol);
#endif
        waitForMove();
    }

    switch (*result)
    {
    case 0:
        printf(DRAW_MESSAGE);
        break;
    case 1:
    case 2:
        printf(WINS_PLAYER_MESSAGE,
               *result, 
               pids[*result]);
        break;
    }

    notifyGameEnded();
    exit(EXIT_SUCCESS);
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
}

void disposeMemory()
{
    disposeSharedMemory(matId);
    disposeSharedMemory(pidId);
    disposeSharedMemory(resId);
    disposeSharedMemory(symId);
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
    sigdelset(&set, SIGUSR1);
    sigdelset(&set, SIGUSR2);
    sigprocmask(SIG_SETMASK, &set, NULL);

    if (signal(SIGINT, exitHandler) == SIG_ERR ||
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

        if (++playersCount == 1)
            printf(A_PLAYER_JOINED_SERVER_MESSAGE, pids[PLAYER_ONE], playerOneSymbol);
        else if (playersCount == 2)
            printf(ANOTHER_PLAYER_JOINED_SERVER_MESSAGE, pids[PLAYER_TWO], playerTwoSymbol);
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
    printf(WAITING_FOR_MOVE_SERVER_MESSAGE, turn == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, turn, FNRM, pids[turn]);

    do
    {
        errno = 0;
        waitSemaphore(semId, WAIT_FOR_MOVE, 1);
    } while (errno == EINTR);
    printf(MOVE_RECEIVED_SERVER_MESSAGE, turn, pids[turn]);

    turn = turn == 1 ? 2 : 1;
    signalSemaphore(semId, turn, 1);
}