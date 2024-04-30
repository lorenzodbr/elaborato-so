#include "data.c"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

void printWelcomeMessageServer();
void printWelcomeMessageClient();
void printLoadingMessage();
void *loadingSpinner();
void errExit(const char *msg);
void printAndFlush(const char *msg);
void startLoadingSpinner();
void stopLoadingSpinner();

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

void printBoard(char *matId){
    printf("\n");
    for(int i = 0; i < MATRIX_SIZE; i++){
        for(int j = 0; j < MATRIX_SIZE; j++){
            printf("%c ", matId[i * MATRIX_SIZE + j]);
        }
        printf("\n");
    }
    printf("\n");
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

void errExit(const char *msg)
{
    printAndFlush(KRED ERROR_CHAR);
#if DEBUG
    perror(msg);
#else
    printf("%s\n", msg);
#endif
    exit(EXIT_FAILURE);
}