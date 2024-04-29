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
    printf(BOLD KGRN TRIS_ASCII_ART_CLIENT);
    printf(CREDITS);
    printf(NO_BOLD KNRM);
}

void printLoadingMessage()
{
    printf(LOADING_MESSAGE KGRN);
    // fflush(stdout);
    // startLoadingSpinner();
}

void printLoadingCompleteMessage()
{
    printf(KNRM "Caricamento completato.\n");
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

void errExit(const char *msg)
{
    printAndFlush(KRED ERROR_CHAR);
    perror(msg);
    exit(EXIT_FAILURE);
}