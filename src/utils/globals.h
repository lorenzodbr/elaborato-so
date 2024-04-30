#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "semaphores/semaphores.h"

typedef struct
{
    int row;
    int col;
} move_t;

void printWelcomeMessageServer()
{
    printf(CLEAR_SCREEN);
    printf(BOLD KCYN TRIS_ASCII_ART_SERVER);
    printf(CREDITS);
    printf(NO_BOLD KNRM);
}

void printWelcomeMessageClient(char *username)
{
    printf(CLEAR_SCREEN);
    printf(BOLD KCYN TRIS_ASCII_ART_CLIENT);
    printf(CREDITS);
    printf(NO_BOLD KNRM "\nBenvenuto, %s! ", username);
}

void printLoadingMessage()
{
    printf(LOADING_MESSAGE KGRN);

#if DEBUG
    printf(SUCCESS_CHAR "PID = %d\n", getpid());
#endif
}

void printLoadingCompleteMessage()
{
    printf(KNRM "Caricamento completato.\n");
}

void initBoard(int *matrix)
{
    for (int i = 0; i < MATRIX_SIDE_LEN; i++)
    {
        for (int j = 0; j < MATRIX_SIDE_LEN; j++)
        {
            matrix[i * MATRIX_SIDE_LEN + j] = 0;
        }
    }

#if DEBUG
    printf(KGRN SUCCESS_CHAR "Matrice inizializzata.\n" KNRM);
#endif
}

void printBoard(int *matrix, char playerOneSymbol, char playerTwoSymbol)
{
    char chars[3] = {' ', playerOneSymbol, playerTwoSymbol};

    printf("\n\n     A   B   C\n\n");
    for (int i = 0; i < MATRIX_SIDE_LEN; i++)
    {
        printf(" %d  ", i + 1);

        for (int j = 0; j < MATRIX_SIDE_LEN; j++)
        {
            printf(" %c", chars[matrix[i * MATRIX_SIDE_LEN + j]]);
            if (j < MATRIX_SIDE_LEN - 1)
            {
                printf(" |");
            }
        }
        if (i < MATRIX_SIDE_LEN - 1)
        {
            printf("\n    -----------\n");
        }
    }
    printf("\n\n");
}

void printAndFlush(const char *msg)
{
    printf("%s", msg);
    fflush(stdout);
}

void printSuccess(const char *msg)
{
    printAndFlush(KGRN SUCCESS_CHAR);
    printAndFlush(msg);
}

void initPids(int *pidsPointer)
{
    for (int i = 0; i < 3; i++)
    {
        pidsPointer[i] = 0;
    }
}

// set specified pid to the first empty slot (out of 3)
int setPid(int *pidsPointer, int pid)
{
    int semId = getSemaphores(N_SEM), playerIndex = -1;

    waitSemaphore(semId, LOCK, 1);
    for (int i = 0; i < 3; i++)
    {
        if (pidsPointer[i] == 0)
        {
            pidsPointer[i] = pid;
            playerIndex = i;
            break;
        }
    }
    signalSemaphore(semId, LOCK, 1);

    return playerIndex;
}

void setPidAt(int *pidsPointer, int index, int pid)
{
    int semId = getSemaphores(N_SEM);

    waitSemaphore(semId, LOCK, 1);
    pidsPointer[index] = pid;
    signalSemaphore(semId, LOCK, 1);
}

int getPid(int *pidsPointer, int index)
{
    return pidsPointer[index];
}

bool isValidMove(int *matrix, char *input, move_t *move)
{
    // the input is valid if it is in the format [A-C or a-c],[1-3]

    if (strlen(input) != 2)
    {
        return false;
    }

    if (((input[0] < 'A' || input[0] > 'C') &&
         (input[0] < 'a' || input[0] > 'c')) ||
        input[1] < '1' || input[1] > '3')
    {
        return false;
    }

    move->row = input[0] - 'A';

    if (input[0] >= 'a' && input[0] <= 'c')
    {
        move->row = input[0] - 'a';
    }

    move->col = input[1] - '1';

    if (matrix[move->col * MATRIX_SIDE_LEN + move->row] != 0)
    {
        return false;
    }

    return true;
}

int isGameEnded(int *matrix)
{
    // check rows
    for (int i = 0; i < MATRIX_SIDE_LEN; i++)
    {
        if (matrix[i * MATRIX_SIDE_LEN] != 0 &&
            matrix[i * MATRIX_SIDE_LEN] == matrix[i * MATRIX_SIDE_LEN + 1] &&
            matrix[i * MATRIX_SIDE_LEN] == matrix[i * MATRIX_SIDE_LEN + 2])
        {
            return matrix[i * MATRIX_SIDE_LEN];
        }
    }

    // check columns
    for (int i = 0; i < MATRIX_SIDE_LEN; i++)
    {
        if (matrix[i] != 0 && matrix[i] == matrix[i + MATRIX_SIDE_LEN] &&
            matrix[i] == matrix[i + 2 * MATRIX_SIDE_LEN])
        {
            return matrix[i];
        }
    }

    // check diagonals
    if (matrix[0] != 0 &&
        matrix[0] == matrix[4] &&
        matrix[0] == matrix[8])
    {
        return matrix[0];
    }

    if (matrix[2] != 0 &&
        matrix[2] == matrix[4] &&
        matrix[2] == matrix[6])
    {
        return matrix[2];
    }

    // check draw
    for (int i = 0; i < MATRIX_SIZE; i++)
    {
        if (matrix[i] == 0)
        {
            return NOT_FINISHED;
        }
    }

    return DRAW;
}