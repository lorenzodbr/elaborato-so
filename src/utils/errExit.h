#include "data.h"

void errExit(const char *msg)
{
    printf(KRED ERROR_CHAR "Errore: %s", msg);

#if DEBUG
    printf(" (errno = %d)\n", errno);
#else
    printf("\n");
#endif

    exit(EXIT_FAILURE);
}