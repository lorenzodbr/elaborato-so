/************************************
 * VR487434 - Lorenzo Di Berardino
 * VR486588 - Filippo Milani
 * 09/05/2024
 ************************************/

#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdbool.h>
#include "../globals.h"

int get_and_init_shared_memory(int, int);
int get_shared_memory(int, int);
void dispose_shared_memory(int);
void* attach_shared_memory(int);
void detach_shared_memory(void*);
bool is_another_server_running(int*, tris_game_t**);

#endif