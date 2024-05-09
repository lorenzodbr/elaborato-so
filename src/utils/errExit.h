/************************************
* VR487434 - Lorenzo Di Berardino
* VR486588 - Filippo Milani
* 09/05/2024
*************************************/

#include "data.h"

void errExit(const char* msg) {
    printf(FRED ERROR_CHAR "%s", msg);

#if DEBUG
    printf(" (errno = %d)\n", errno);
#else
    printf("\n");
#endif

    exit(EXIT_FAILURE);
}