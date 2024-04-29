#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "utils/data.c"
#include "utils/globals.c"
#include "utils/shared_memory/shared_memory.c"
#include "utils/semaphores/semaphores.c"

void init();
void initMatrix();
void initSemaphores();
void setAtExitCleanup();
void waitForPlayers();
void disposeMatrix();
void disposeSemaphores();

int shmId;
char *matrix;
int semId;

int timeout;
char playerOneSymbol;
char playerTwoSymbol;

int main(int argc, char *argv[])
{
    if (argc != N_ARGS)
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
    initMatrix();
    initSemaphores();
    setAtExitCleanup();
    printLoadingCompleteMessage();
}

void initMatrix()
{
    shmId = getSharedMemory(MATRIX_SIZE * MATRIX_SIZE * sizeof(char));

#if DEBUG
    printf(SUCCESS_CHAR "Memoria condivisa ottenuta (ID: %d).\n", shmId);
#endif

    matrix = (char *)attachSharedMemory(shmId);

#if DEBUG
    printf(SUCCESS_CHAR "Memoria condivisa agganciata (@ %p).\n", matrix);
#endif

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
}

void disposeMatrix()
{
    disposeSharedMemory(shmId);

#if DEBUG
    printf(KGRN SUCCESS_CHAR "Matrice deallocata.\n");
#endif
}

void initSemaphores()
{
    semId = getSemaphores(1);
    setSemaphore(semId, 0, 0);

#if DEBUG
    printf(SUCCESS_CHAR "Semafori inizializzati.\n");
#endif
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

void waitForPlayers()
{
    printf("\nAttendo i giocatori...\n");
    waitSemaphore(semId, 0, 1);

    printf("Un giocatore (%c) è entrato in partita.\n", playerOneSymbol);
    waitSemaphore(semId, 0, 1);

    printf("Un altro giocatore (%c) è entrato in partita.\nPronti per cominciare.\n\n", playerTwoSymbol);
}