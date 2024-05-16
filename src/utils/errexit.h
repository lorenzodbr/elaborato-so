/************************************
 * VR487434 - Lorenzo Di Berardino
 * VR486588 - Filippo Milani
 * 09/05/2024
 ************************************/

#include "data.h"

/// @brief Print an error message and exit the program
/// @param msg The error message to print
void errexit(const char* msg)
{
    printf(FRED ERROR_CHAR "%s", msg);

#if DEBUG
    printf(" (errno = %d)\n", errno);
#else
    printf("\n");
#endif

    fflush(stdout);

    exit(EXIT_FAILURE);
}