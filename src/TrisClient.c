#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>

#include "utils/data.h"
#include "utils/globals.h"
#include "utils/shared_memory/shared_memory.h"

void init();
void notifyPlayerReady();
void initSharedMemory();
void initSemaphores();
void askForInput();
void waitForOpponent();
void initSignals();
void exitHandler(int sig);
void checkResults(int sig);
void serverQuitHandler(int sig);

int semId;
int matId;
int pidId;

char *matrix;
pid_t *pid;

bool firstCTRLCPressed = false;
int playerIndex = -1;

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

    initSemaphores();
    initSharedMemory();
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

    playerIndex = setPid(pid, getpid());

    printf(KGRN SUCCESS_CHAR "Trovato TrisServer con PID = %d\n" KNRM, *pid);
}

void initSemaphores()
{
    semId = getSemaphores(N_SEM);
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

    if (signal(SIGUSR1, serverQuitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGUSR2, checkResults) == SIG_ERR)
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

void checkResults(int sig){
    printf("Ha vinto...\n");
    exit(EXIT_SUCCESS);
}

void exitHandler(int sig)
{
    if (firstCTRLCPressed)
    {
        kill(pid[SERVER], playerIndex == 1 ? SIGUSR1 : SIGUSR2);
        exit(EXIT_SUCCESS);
    }
    firstCTRLCPressed = true;
    printf("\nPremi CTRL+C di nuovo per uscire.\n");
}

void serverQuitHandler(int sig)
{
    printf(KRED "\nIl server ha terminato la partita.\n");
    exit(EXIT_FAILURE);
}