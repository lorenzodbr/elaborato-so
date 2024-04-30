#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "../data.c"
#include <errno.h>

#ifndef SEMUN_H
#define SEMUN_H
union semun
{
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
#endif

int getSemaphores(int nsems);
void setSemaphore(int semid, int semnum, int value);
void setSemaphores(int semid, int nsems, short unsigned *values);

int getSemaphores(int nsems)
{
    int semId = semget(ftok(FTOK_PATH, SEM_ID), nsems, IPC_CREAT | 0640);
    if (semId < 0)
    {
        errExit("semget");
    }

#if DEBUG
    printf(SUCCESS_CHAR "Semafori ottenuti.\n");
#endif

    return semId;
}

void setSemaphore(int semid, int semnum, int value)
{
    if (semctl(semid, semnum, SETVAL, value) < 0)
    {
        errExit("semctl");
    }

#if DEBUG
    printf(SUCCESS_CHAR "Semafori inizializzati.\n");
#endif
}

void setSemaphores(int semid, int nsems, short unsigned *values)
{
    union semun arg;
    arg.array = values;

    if (semctl(semid, 0, SETALL, arg) < 0)
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

    int semopRet = semop(semid, &sops, 1);
    if (semopRet < 0 && errno != EINTR)
    {
        errExit("semop");
    }
}

void signalSemaphore(int semid, int semnum, int value)
{
    struct sembuf sops = {semnum, value, 0};

    int semopRet = semop(semid, &sops, 1);
    if (semopRet < 0 && errno != EINTR)
    {
        errExit("semop");
    }
}