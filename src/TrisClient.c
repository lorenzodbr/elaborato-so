#include <stdio.h>
#include <stdlib.h>
#include "utils/globals.c"
#include "utils/semaphores/semaphores.c"
#include "utils/shared_memory/shared_memory.c"

void init();
void waitForServer();
void initSharedMemory();
void initSemaphore();
void askForInput();

int semId;
int matId;
int pidId;

char *matrix;
pid_t *pid;

int main()
{
    init();
    waitForServer();

    askForInput();
    return 0;
}

void init()
{
    printWelcomeMessageClient();
    printLoadingMessage();

    initSharedMemory();
    initSemaphore();
}

void initSharedMemory()
{
    matId = getSharedMemory(MATRIX_SIZE, MATRIX_ID);

    if (matId < 0)
    {
        printf(KRED "Nessun server trovato. Esegui TrisServer prima di eseguire TrisClient.\n");
        exit(EXIT_FAILURE);
    }

    matrix = (char *)attachSharedMemory(matId);

#if DEBUG
    printf(SUCCESS_CHAR "Memoria di gioco agganciata (@ %p).\n", matrix);
#endif

    pidId = getSharedMemory(PID_SIZE, PID_ID);

    if (pidId < 0)
    {
        printf(KRED "Nessun server trovato. Esegui TrisServer prima di eseguire TrisClient.\n");
        exit(EXIT_FAILURE);
    }

    pid = (int *)attachSharedMemory(pidId);

    printf(SUCCESS_CHAR "Trovato TrisServer con PID = %d\n", *pid);
}

void initSemaphore()
{
    semId = getSemaphores(1);

#ifdef DEBUG
    printSuccess("Semaforo ottenuto.\n");
#endif
}

void waitForServer()
{
    signalSemaphore(semId, 0, 1);
}

void askForInput()
{
    int input;
    scanf("%d", &input);
}