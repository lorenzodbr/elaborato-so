/************************************
 * VR487434 - Lorenzo Di Berardino
 * VR486588 - Filippo Milani
 * 09/05/2024
 ************************************/

#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>

#include "../errexit.h"

#ifndef SEMUN_H
#define SEMUN_H
union semun {
    int val;
    struct semid_ds* buf;
    unsigned short* array;
};
#endif

int get_semaphores(int);
void set_semaphore(int, int, int);
void set_semaphores(int, int, short unsigned*);
void dispose_semaphore(int);
void wait_semaphore(int, int, int);
void signal_semaphore(int, int, int);

int get_semaphores(int n_sems)
{
    int sem_id = semget(SEM_ID, n_sems, IPC_CREAT | PERM);
    if (sem_id < 0) {
#if DEBUG
        errexit(SEMAPHORE_INITIALIZATION_ERROR);
#else
        exit(EXIT_FAILURE);
#endif
    }

#if DEBUG
    printf(SEMAPHORE_OBTAINED_SUCCESS, sem_id);
#endif

    return sem_id;
}

void set_semaphore(int sem_id, int sem_num, int value)
{
    if (semctl(sem_id, sem_num, SETVAL, value) < 0) {
#if DEBUG
        errexit(SEMAPHORE_INITIALIZATION_ERROR);
#else
        exit(EXIT_FAILURE);
#endif
    }

#if DEBUG
    printf(SEMAPHORES_INITIALIZED_SUCCESS);
#endif
}

void set_semaphores(int sem_id, int n_sems, short unsigned* values)
{
    union semun arg;
    arg.array = values;

    if (semctl(sem_id, 0, SETALL, arg) < 0) {
#if DEBUG
        errexit(SEMAPHORE_INITIALIZATION_ERROR);
#else
        exit(EXIT_FAILURE);
#endif
    }

#if DEBUG
    printf(SEMAPHORES_INITIALIZED_SUCCESS);
#endif
}

void dispose_semaphore(int sem_id)
{
    if (semctl(sem_id, 0, IPC_RMID) < 0) {
#if DEBUG
        errexit(SEMAPHORE_DEALLOCATION_ERROR);
#else
        exit(EXIT_FAILURE);
#endif
    }

#if DEBUG
    printf(SEMAPHORES_DISPOSED_SUCCESS);
#endif
}

void wait_semaphore(int sem_id, int sem_num, int value)
{
    if (value > 0)
        value *= -1;

    struct sembuf sops = { sem_num, value, 0 };

    int semop_ret = semop(sem_id, &sops, 1);
    if (semop_ret < 0 && (errno != EINTR && errno != EIDRM)) {
#if DEBUG
        errexit(SEMAPHORE_WAITING_ERROR);
#else
        exit(EXIT_FAILURE);
#endif
    }
}

void signal_semaphore(int sem_id, int sem_num, int value)
{
    struct sembuf sops = { sem_num, value, 0 };

    int semop_ret = semop(sem_id, &sops, 1);
    if (semop_ret < 0 && (errno != EINTR || errno != EIDRM)) {
#if DEBUG
        errexit(SEMAPHORE_SIGNALING_ERROR);
#else
        exit(EXIT_FAILURE);
#endif
    }
}