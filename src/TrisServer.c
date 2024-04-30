#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>

#include "utils/data.c"
#include "utils/globals.c"
#include "utils/shared_memory/shared_memory.c"
#include "utils/semaphores/semaphores.c"

void init();
void initSharedMemory();
void initSemaphores();
void setAtExitCleanup();
void waitForPlayers();
void disposeMatrix();
void disposeSemaphores();
void initSignals();
void notifyOpponentReady();
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
    initSharedMemory();
    initSemaphores();
    initSignals();
    printLoadingCompleteMessage();
}

void initSharedMemory()
{
    matId = getAndInitSharedMemory(MATRIX_SIZE, MATRIX_ID);
    matrix = (char *)attachSharedMemory(matId);
    initBoard(matrix);

#if DEBUG
    printf(SUCCESS_CHAR "Matrice inizializzata.\n");
#endif

    pidId = getAndInitSharedMemory(PID_SIZE, PID_ID);
    pid = (int *)attachSharedMemory(pidId);
    *pid = getpid();
}

void disposeMatrix()
{
    disposeSharedMemory(matId);

#if DEBUG
    printf(KGRN SUCCESS_CHAR "Matrice deallocata.\n");
#endif
}

void initSemaphores()
{
    semId = getSemaphores(2);

    short unsigned values[2];
    values[WAIT_FOR_PLAYERS] = 0;
    values[WAIT_FOR_OPPONENT] = 0;

    setSemaphores(semId, 2, values);
}

void disposeSemaphores()
{
    disposeSemaphore(semId);

#if DEBUG
    printf(SUCCESS_CHAR "Semafori deallocati.\n");
#endif
}

void setAtExitCleanup()
{
    if (atexit(disposeSemaphores))
    {
        errExit("atexit");
    }
    if (atexit(disposeMatrix))
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
}

void exitHandler(int sig)
{
    if (firstCTRLCPressed)
    {
        exit(EXIT_SUCCESS);
    }
    firstCTRLCPressed = true;
    printf("\nPremi CTRL+C di nuovo per uscire.\n");
}

void playerQuitHandler(int sig)
{
    printf("\nUn giocatore ha abbandonato la partita...\n");
    playersCount--;

    if(started){
        printf("Partita terminata.\n");
        exit(EXIT_SUCCESS);
    }
}

void waitForPlayers()
{
    printf("\nAttendo i giocatori...\n");
    while(playersCount < 2){
        do
        {
            errno = 0;
            waitSemaphore(semId, WAIT_FOR_PLAYERS, 1);
        } while (errno == EINTR);

        if(++playersCount == 1)
            printf("Un giocatore (%c) è entrato in partita.\n", playerOneSymbol);
        else 
            printf("Un altro giocatore (%c) è entrato in partita. Pronti per cominciare.\n", playerTwoSymbol);
    }

    signalSemaphore(semId, WAIT_FOR_OPPONENT, 2);
}

void notifyOpponentReady()
{
    signalSemaphore(semId, WAIT_FOR_OPPONENT, 2);
}