#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include "utils/globals.c"
#include "utils/semaphores/semaphores.c"
#include "utils/shared_memory/shared_memory.c"

void init();
void notifyPlayerReady();
void initSharedMemory();
void initSemaphores();
void askForInput();
void waitForOpponent();
void initSignals();
void exitHandler(int sig);

int semId;
int matId;
int pidId;

char *matrix;
pid_t *pid;

bool firstCTRLCPressed = false;

int main()
{
    init();
    notifyPlayerReady();
    waitForOpponent();
    do
    {
        printBoard(matrix);
        askForInput();
    } while (1);
    return 0;
}

void init()
{
    printWelcomeMessageClient();
    printLoadingMessage();

    initSharedMemory();
    initSemaphores();
    initSignals();
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

    pidId = getSharedMemory(PID_SIZE, PID_ID);

    if (pidId < 0)
    {
        printf(KRED "Nessun server trovato. Esegui TrisServer prima di eseguire TrisClient.\n");
        exit(EXIT_FAILURE);
    }

    pid = (int *)attachSharedMemory(pidId);

    printf(SUCCESS_CHAR "Trovato TrisServer con PID = %d\n", *pid);
}

void initSemaphores()
{
    semId = getSemaphores(2);

#ifdef DEBUG
    printSuccess("Semaforo ottenuto.\n");
#endif
}

void initSignals(){
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGINT);
    sigprocmask(SIG_SETMASK, &set, NULL);

    if (signal(SIGINT, exitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }
}

void notifyPlayerReady()
{
    signalSemaphore(semId, WAIT_FOR_PLAYERS, 1);
}

void waitForOpponent()
{
    printf(KNRM "In attesa dell'avversario...\n");
    waitSemaphore(semId, WAIT_FOR_OPPONENT, 1);
    printf("Avversario pronto!\n");
}

void askForInput()
{
    char input[50];

    printf("Inserisci una mossa: ");
    scanf("%s", input);
}

void exitHandler(int sig)
{
    if (firstCTRLCPressed)
    {
        kill(*pid, SIGUSR1);
        exit(EXIT_SUCCESS);
    }
    firstCTRLCPressed = true;
    printf("\nPremi CTRL+C di nuovo per uscire.\n");
}