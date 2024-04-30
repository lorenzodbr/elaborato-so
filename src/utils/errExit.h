#include "data.h"

void errExit(const char *msg)
{
    printf(KRED ERROR_CHAR "Errore: %s (errno = %d)\n", msg, errno);
    exit(EXIT_FAILURE);
}