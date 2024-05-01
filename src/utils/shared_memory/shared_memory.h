#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>

int getAndInitSharedMemory(int size, int id)
{
    int shmid = shmget(id, size, IPC_CREAT | PERM);
    if (shmid < 0)
    {
        errExit(SHARED_MEMORY_ALLOCATION_ERROR);
    }

#if DEBUG
    printf(SHARED_MEMORY_OBTAINED_SUCCESS, shmid);
#endif

    return shmid;
}

int getSharedMemory(int size, int id)
{
    int shmid = shmget(id, size, PERM);

    return shmid;
}

void disposeSharedMemory(int shmid)
{
    if (shmctl(shmid, IPC_RMID, NULL) < 0)
    {
        errExit(SHARED_MEMORY_DEALLOCATION_ERROR);
    }

#if DEBUG
    printf(SHARED_MEMORY_DISPOSED_SUCCESS)
#endif
}

void *attachSharedMemory(int shmid)
{
    void *addr = shmat(shmid, NULL, 0);
    if (addr == (void *)-1)
    {
        errExit(SHARED_MEMORY_ATTACH_ERROR);
    }

#if DEBUG
    printf(SHARED_MEMORY_ATTACHED_SUCCESS, addr);
#endif

    return addr;
}