#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>

int getSharedMemory(int size){
    int shmid = shmget(ftok(FTOK_PATH, PROJ_ID), size, IPC_CREAT | 0640);
    if(shmid < 0){
        errExit("shmget");
    }

    return shmid;
}

void disposeSharedMemory(int shmid){
    if(shmctl(shmid, IPC_RMID, NULL) < 0){
        errExit("shmctl");
    }
}

void *attachSharedMemory(int shmid){
    void *addr = shmat(shmid, NULL, 0);
    if(addr == (void *)-1){
        errExit("shmat");
    }

    return addr;
}