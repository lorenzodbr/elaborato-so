#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include "../data.c"

int getAndInitSharedMemory(int size, int id)
{
    int shmid = shmget(ftok(FTOK_PATH, id), size, IPC_CREAT | 0640);
    if (shmid < 0)
    {
        errExit("shmget");
    }

#if DEBUG
    printf(SUCCESS_CHAR "Memoria condivisa ottenuta (ID: %d).\n", shmid);
#endif

    return shmid;
}

int getSharedMemory(int size, int id)
{
    int shmid = shmget(ftok(FTOK_PATH, id), size, 0640);

    return shmid;
}

void disposeSharedMemory(int shmid)
{
    if (shmctl(shmid, IPC_RMID, NULL) < 0)
    {
        errExit("shmctl");
    }
}

void *attachSharedMemory(int shmid)
{
    void *addr = shmat(shmid, NULL, 0);
    if (addr == (void *)-1)
    {
        errExit("shmat");
    }

#if DEBUG
    printf(SUCCESS_CHAR "Memoria condivisa agganciata (@ %p).\n", addr);
#endif

    return addr;
}