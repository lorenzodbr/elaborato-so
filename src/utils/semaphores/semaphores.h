#ifndef SEMAPHORES_H
#define SEMAPHORES_H

int get_semaphores(int);
void set_semaphore(int, int, int);
void set_semaphores(int, int, short unsigned*);
void dispose_semaphore(int);
void wait_semaphore(int, int, int);
void signal_semaphore(int, int, int);

#endif