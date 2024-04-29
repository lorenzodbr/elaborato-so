#include "data.c"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

void printWelcomeMessage();
void printLoadingMessage();
void *loadingSpinner();
void errExit(const char *msg);
void printAndFlush(const char *msg);
void startLoadingSpinner();

bool endSpinner = false;

void printWelcomeMessage()
{
    printf(CLEAR_SCREEN);
    printf("%s%s\n", KGRN, TRIS_ASCII_ART);
    printf("%s%s\n", KGRN, CREDITS);
}

void printLoadingMessage()
{
    printf(LOADING_MESSAGE);
    fflush(stdout);
    startLoadingSpinner();

    sleep(3);

    endSpinner = true;

    pthread_exit(NULL);
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
    perror(msg);
    exit(EXIT_FAILURE);
}