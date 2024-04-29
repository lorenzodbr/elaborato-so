#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "../data.c"

int getSemaphores(int nsems);
void setSemaphore(int semid, int semnum, int value);

int getSemaphores(int nsems)
{
    int semId = semget(ftok(FTOK_PATH, PROJ_ID), nsems, IPC_CREAT | 0640);
    if (semId < 0)
    {
        errExit("semget");
    }

    return semId;
}

void setSemaphore(int semid, int semnum, int value)
{
    if (semctl(semid, semnum, SETVAL, value) < 0)
    {
        errExit("semctl");
    }
}

void disposeSemaphore(int semid)
{
    if (semctl(semid, 0, IPC_RMID) < 0)
    {
        errExit("semctl");
    }
}

void waitSemaphore(int semid, int semnum, int value)
{
    if (value > 0)
        value *= -1;

    struct sembuf sops = {semnum, value, 0};
    if (semop(semid, &sops, 1) < 0)
    {
        errExit("semop");
    }
}

void signalSemaphore(int semid, int semnum, int value)
{
    struct sembuf sops = {semnum, value, 0};
    if (semop(semid, &sops, 1) < 0)
    {
        errExit("semop");
    }
}