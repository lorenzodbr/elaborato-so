#include <stdio.h>
#include <stdlib.h>
#include "utils/globals.c"
#include "utils/semaphores/semaphores.c"

void init();
void waitForServer();
void initSemaphore();
void askForInput();

int semId;

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

    initSemaphore();
}

void initSemaphore()
{
    semId = getSemaphores(1);
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