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
void notifyPlayerWhoWon(int playerWhoWon);
void notifyServerQuit();
void exitHandler(int sig);
void playerQuitHandler(int sig);

int matId;
char *matrix;
int pidId;
int *pid;
int semId;

int timeout;
char playerOneSymbol;
char playerTwoSymbol;

bool firstCTRLCPressed = false;
bool started = false;
int playersCount = 0;

int main(int argc, char *argv[])
{
    if (argc != N_ARGS_SERVER)
    {
        errExit(USAGE_ERROR);
    }

    timeout = atoi(argv[1]);
    playerOneSymbol = argv[2][0];
    playerTwoSymbol = argv[3][0];

    init();
    waitForPlayers();
    notifyOpponentReady();

    started = true;

    while (1)
    {
        printBoard(matrix);
        sleep(100);
    }

    return EXIT_SUCCESS;
}

void init()
{
    // Messages
    printWelcomeMessageServer();
    printLoadingMessage();

    // Data structures
    setAtExitCleanup();
    initSemaphores();
    initSharedMemory();
    initSignals();

    printLoadingCompleteMessage();
}

void initSharedMemory()
{
    matId = getAndInitSharedMemory(MATRIX_SIZE, MATRIX_ID);
    matrix = (char *)attachSharedMemory(matId);
    initBoard(matrix);

    pidId = getAndInitSharedMemory(PID_SIZE, PID_ID);
    pid = (int *)attachSharedMemory(pidId);
    initPids(pid);
    setPidAt(pid, 0, getpid());
}

void disposeMemory()
{
    disposeSharedMemory(matId);
    disposeSharedMemory(pidId);
}

void initSemaphores()
{
    semId = getSemaphores(N_SEM);

    short unsigned values[N_SEM];
    values[WAIT_FOR_PLAYERS] = 0;
    values[WAIT_FOR_OPPONENT] = 0;
    values[LOCK] = 1;
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

    printf("\nIl giocatore %d (con PID = %d) ha abbandonato la partita.\n", playerWhoQuitted, pid[playerWhoQuitted]);
    setPidAt(pid, playerWhoQuitted, 0);
    playersCount--;

    if (started)
    {
        printf("Partita terminata: vince il giocatore %d (con PID = %d).\n", playerWhoStayed, pid[playerWhoStayed]);
        notifyPlayerWhoWon(playerWhoStayed);
        exit(EXIT_SUCCESS);
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
    signalSemaphore(semId, WAIT_FOR_OPPONENT, 2);
}

void notifyPlayerWhoWon(int playerWhoWon)
{
    kill(pid[playerWhoWon], SIGUSR2);
}

void notifyServerQuit()
{
    kill(pid[PLAYER_ONE], SIGUSR1);
    kill(pid[PLAYER_TWO], SIGUSR1);
}