/************************************
* VR487434 - Lorenzo Di Berardino
* VR486588 - Filippo Milani
* 09/05/2024
*************************************/

#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>

int get_and_init_shared_memory(int size, int id) {
    int shm_id = shmget(id, size, IPC_CREAT | PERM);
    if (shm_id < 0) {
        errexit(SHARED_MEMORY_ALLOCATION_ERROR);
    }

#if DEBUG
    printf(SHARED_MEMORY_OBTAINED_SUCCESS, shm_id);
#endif

    return shm_id;
}

int get_shared_memory(int size, int id) {
    int shm_id = shmget(id, size, PERM);

    return shm_id;
}

void dispose_shared_memory(int shm_id) {
    if (shmctl(shm_id, IPC_RMID, NULL) < 0) {
        errexit(SHARED_MEMORY_DEALLOCATION_ERROR);
    }

#if DEBUG
    printf("\n" SHARED_MEMORY_DEALLOCATION_SUCCESS);
#endif
}

void* attach_shared_memory(int shm_id) {
    void* addr = shmat(shm_id, NULL, 0);
    if (addr == (void*)-1) {
        errexit(SHARED_MEMORY_ATTACH_ERROR);
    }

#if DEBUG
    printf(SHARED_MEMORY_ATTACHED_SUCCESS, addr);
#endif

    return addr;
}

void detach_shared_memory(void* addr) {
    if (shmdt(addr) < 0) {
        errexit(SHARED_MEMORY_DETACH_ERROR);
    }
}

bool are_there_attached_processes() {
    // check if there are attached processes by checking
    // if the shared memory is obtainable without creating it
    // (only the first server is capable of this)
    return get_shared_memory(0, GAME_ID) > -1;
}