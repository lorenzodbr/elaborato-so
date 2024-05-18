/************************************
 * VR487434 - Lorenzo Di Berardino
 * VR486588 - Filippo Milani
 * 09/05/2024
 ************************************/

#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

int get_and_init_shared_memory(int size, int id)
{
    int shm_id = shmget(id, size, IPC_CREAT | PERM);
    if (shm_id < 0)
        errexit(SHARED_MEMORY_ALLOCATION_ERROR);

#if DEBUG
    printf(SHARED_MEMORY_OBTAINED_SUCCESS, shm_id);
#endif

    return shm_id;
}

int get_shared_memory(int size, int id)
{
    int shm_id = shmget(id, size, PERM);

    return shm_id;
}

void dispose_shared_memory(int shm_id)
{
    if (shmctl(shm_id, IPC_RMID, NULL) < 0)
        errexit(SHARED_MEMORY_DEALLOCATION_ERROR);

#if DEBUG
    printf("\n" SHARED_MEMORY_DEALLOCATION_SUCCESS);
#endif
}

void* attach_shared_memory(int shm_id)
{
    void* addr = shmat(shm_id, NULL, 0);
    if (addr == (void*)-1)
        errexit(SHARED_MEMORY_ATTACH_ERROR);

#if DEBUG
    printf(SHARED_MEMORY_ATTACHED_SUCCESS, addr);
#endif

    return addr;
}

void detach_shared_memory(void* addr)
{
    if (shmdt(addr) < 0)
        errexit(SHARED_MEMORY_DETACH_ERROR);
}

bool is_another_server_running(int* game_id, tris_game_t** game)
{
    // check if there are attached processes by trying to attach to the
    // shared memory if the shared memory is obtainable without creating it
    // (only the first server is capable of this)
    *game_id = get_shared_memory(0, GAME_ID);

    // if the shared memory does not exist, no server is running
    if (*game_id < 0)
        return false;

    // if the shared memory exists, check if the set pid corresponds to a running process
    *game = (tris_game_t*)attach_shared_memory(*game_id);

    if (kill((*game)->pids[SERVER], 0) < 0)
        return false;

    return true;
}