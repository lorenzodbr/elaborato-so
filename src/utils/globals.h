#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "semaphores/semaphores.h"

void printWelcomeMessageServer();
void printWelcomeMessageClient();
void printLoadingMessage();
void *loadingSpinner();
void printAndFlush(const char *msg);
void startLoadingSpinner();
void stopLoadingSpinner();
void printSuccess(const char *msg);
void initBoard(char *matId);
void printBoard(char *matId);
void initPids(int *pidsPointer);
int setPid(int *pidsPointer, int pid);
int getPid(int *pidsPointer, int index);
void setPidAt(int *pidsPointer, int pid, int index);

bool endSpinner = false;

void printWelcomeMessageServer()
{
    printf(CLEAR_SCREEN);
    printf(BOLD KCYN TRIS_ASCII_ART_SERVER);
    printf(CREDITS);
    printf(NO_BOLD KNRM);
}

void printWelcomeMessageClient()
{
    printf(CLEAR_SCREEN);
    printf(BOLD KCYN TRIS_ASCII_ART_CLIENT);
    printf(CREDITS);
    printf(NO_BOLD KNRM);
}

void printLoadingMessage()
{
    printf(LOADING_MESSAGE KGRN);

#ifdef DEBUG
    printf(SUCCESS_CHAR "PID = %d\n", getpid());
#endif
}

void printLoadingCompleteMessage()
{
    printf(KNRM "Caricamento completato.\n");
}

void initBoard(char *matId)
{
    for (int i = 0; i < MATRIX_SIDE_LEN; i++)
    {
        for (int j = 0; j < MATRIX_SIDE_LEN; j++)
        {
            matId[i * MATRIX_SIDE_LEN + j] = ' ';
        }
    }

#if DEBUG
    printf(KGRN SUCCESS_CHAR "Matrice inizializzata.\n" KNRM);
#endif
}

void printBoard(char *matId)
{
    printf("\n");
    for (int i = 0; i < MATRIX_SIDE_LEN; i++)
    {
        printf("  ");

        for (int j = 0; j < MATRIX_SIDE_LEN; j++)
        {
            printf(" %c", matId[i * MATRIX_SIDE_LEN + j]);
            if (j < MATRIX_SIDE_LEN - 1)
            {
                printf(" |");
            }
        }
        if (i < MATRIX_SIDE_LEN - 1)
        {
            printf("\n  -----------\n");
        }
    }
    printf("\n\n");
}

void stopLoadingSpinner()
{
    endSpinner = true;
}

void startLoadingSpinner()
{
    pthread_t tid;
    pthread_create(&tid, NULL, loadingSpinner, NULL);
}

void *loadingSpinner()
{
    while (1)
    {
        printAndFlush("\b|");
        usleep(100000);
        printAndFlush("\b/");
        usleep(100000);
        printAndFlush("\b-");
        usleep(100000);
        printAndFlush("\b\\");
        usleep(100000);

        if (endSpinner)
        {
            printAndFlush("\bOK\n");
            break;
        }
    }

    return NULL;
}

void printAndFlush(const char *msg)
{
    printf("%s", msg);
    fflush(stdout);
}

void printSuccess(const char *msg)
{
    printAndFlush(KGRN SUCCESS_CHAR);
    printAndFlush(msg);
}

void initPids(int *pidsPointer)
{
    for (int i = 0; i < 3; i++)
    {
        pidsPointer[i] = 0;
    }
}

// set specified pid to the first empty slot (out of 3)
int setPid(int *pidsPointer, int pid)
{
    int semId = getSemaphores(N_SEM), playerIndex;

    waitSemaphore(semId, LOCK, 1);
    for (int i = 0; i < 3; i++)
    {
        if (pidsPointer[i] == 0)
        {
            pidsPointer[i] = pid;
            playerIndex = i;
            break;
        }
    }
    signalSemaphore(semId, LOCK, 1);

    return playerIndex;
}

void setPidAt(int *pidsPointer, int index, int pid)
{
    int semId = getSemaphores(N_SEM);

    waitSemaphore(semId, LOCK, 1);
    pidsPointer[index] = pid;
    signalSemaphore(semId, LOCK, 1);
}

int getPid(int *pidsPointer, int index)
{
    return pidsPointer[index];
}