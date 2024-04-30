#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include "../data.c"
#include <errno.h>

int getSemaphores(int nsems);
void setSemaphore(int semid, int semnum, int value);

int getSemaphores(int nsems)
{
    int semId = semget(ftok(FTOK_PATH, MATRIX_ID), nsems, IPC_CREAT | 0640);
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