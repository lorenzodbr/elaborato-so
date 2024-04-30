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

pid_t *pid;
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

    init();
    waitForPlayers();
    notifyOpponentReady();

    started = true;

    printf("Inizia il giocatore %d (con PID = %d).\n", turn, pid[turn]);

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
        printf(KYEL "\nPareggio.\n");
        break;
    case 1:
    case 2:
        printf(KGRN "\nVince il giocatore %d (con PID = %d).\n", *result, pid[*result]);
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
    pid = (int *)attachSharedMemory(pidId);
    initPids(pid);
    setPidAt(pid, 0, getpid());

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
    if (atexit(disposeSemaphores))
    {
        errExit("atexit");
    }
    if (atexit(disposeMemory))
    {
        errExit("atexit");
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

    if (signal(SIGINT, exitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGUSR1, playerQuitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGUSR2, playerQuitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }
}

void exitHandler(int sig)
{
    if (firstCTRLCPressed)
    {
        printf("\nChiusura in corso...\n");
        notifyServerQuit();
        exit(EXIT_SUCCESS);
    }
    firstCTRLCPressed = true;
    printf("\nPremi CTRL+C di nuovo per uscire.\n");
}

void playerQuitHandler(int sig)
{
    int playerWhoQuitted = sig == SIGUSR1 ? PLAYER_ONE : PLAYER_TWO;
    int playerWhoStayed = playerWhoQuitted == PLAYER_ONE ? PLAYER_TWO : PLAYER_ONE;

    printf("\nIl giocatore %d (con PID = %d) ha abbandonato la partita.", playerWhoQuitted, pid[playerWhoQuitted]);
    setPidAt(pid, playerWhoQuitted, 0);
    playersCount--;

    if (started)
    {
        *result = QUIT;
        printf(" Vince il giocatore %d (con PID = %d).\n", playerWhoStayed, pid[playerWhoStayed]);
        notifyPlayerWhoWonForQuit(playerWhoStayed);
        exit(EXIT_SUCCESS);
    }
    else
    {
        printf("\n");
    }
}

void waitForPlayers()
{
    printf("\nAttendo i giocatori...\n");

    while (playersCount < 2)
    {
        do
        {
            errno = 0;
            waitSemaphore(semId, WAIT_FOR_PLAYERS, 1);
        } while (errno == EINTR);

        if (++playersCount == 1)
            printf("Un giocatore con PID = %d (%c) è entrato in partita.\n", pid[PLAYER_ONE], playerOneSymbol);
        else
            printf("Un altro giocatore con PID = %d (%c) è entrato in partita. Pronti per cominciare.\n", pid[PLAYER_TWO], playerTwoSymbol);
    }
}

void notifyOpponentReady()
{
    signalSemaphore(semId, WAIT_FOR_OPPONENT_READY, 2);
}

void notifyPlayerWhoWonForQuit(int playerWhoWon)
{
    kill(pid[playerWhoWon], SIGUSR2);
}

void notifyGameEnded()
{
    kill(pid[PLAYER_ONE], SIGUSR2);
    kill(pid[PLAYER_TWO], SIGUSR2);
}

void notifyServerQuit()
{
    if (pid[PLAYER_ONE] != 0)
        kill(pid[PLAYER_ONE], SIGUSR1);

    if (pid[PLAYER_TWO] != 0)
        kill(pid[PLAYER_TWO], SIGUSR1);
}

void waitForMove()
{
    printf("In attesa della mossa del giocatore %d (con PID = %d)...\n", turn, pid[turn]);

    do
    {
        errno = 0;
        waitSemaphore(semId, WAIT_FOR_MOVE, 1);
    } while (errno == EINTR);
    printf("Mossa ricevuta dal giocatore %d (con PID = %d).\n", turn, pid[turn]);

    turn = turn == 1 ? 2 : 1;
    signalSemaphore(semId, turn, 1);
}