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
void exitHandler(int sig);

int matId;
char *matrix;
int pidId;
int *pid;
int semId;

int timeout;
char playerOneSymbol;
char playerTwoSymbol;

bool firstCTRLCPressed = false;

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

    for (int i = 0; i < MATRIX_SIZE; i++)
    {
        for (int j = 0; j < MATRIX_SIZE; j++)
        {
            matrix[i * MATRIX_SIZE + j] = ' ';
        }
    }

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
    semId = getSemaphores(1);
    setSemaphore(semId, 0, 0);
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

void initSignals(){
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGINT);
    sigprocmask(SIG_SETMASK, &set, NULL);

    if(signal(SIGINT, exitHandler) == SIG_ERR){
        perror("signal");
        exit(EXIT_FAILURE);
    }
}

void exitHandler(int sig){
    if(firstCTRLCPressed){
        exit(EXIT_SUCCESS);
    }
    firstCTRLCPressed = true;
    printf("\nPremi CTRL+C di nuovo per uscire.\n");
}

void waitForPlayers()
{
    printf("\nAttendo i giocatori...\n");
    do {
        errno = 0;
        waitSemaphore(semId, 0, 1);
    } while(errno == EINTR);

    playersCount++;

    printf("Un giocatore (%c) è entrato in partita.\n", playerOneSymbol);

    do {
        errno = 0;
        waitSemaphore(semId, 0, 1);
    } while(errno == EINTR);

    printf("Un altro giocatore (%c) è entrato in partita.\nPronti per cominciare.\n\n", playerTwoSymbol);
}