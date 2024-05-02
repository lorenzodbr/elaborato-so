#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

#include "semaphores/semaphores.h"

typedef struct
{
    int row;
    int col;
} move_t;

typedef struct
{
    int matrix[MATRIX_SIZE];
    pid_t pids[PID_ARRAY_LEN];
    char usernames[USERNAMES_ARRAY_LEN][USERNAME_MAX_LEN];
    int result;
    char symbols[SYMBOLS_ARRAY_LEN];
    int timeout;
} tris_game_t;

void printHeaderServer()
{
    printf(CLEAR_SCREEN);
    printf(TRIS_ASCII_ART_SERVER);
}

void printHeaderClient()
{
    printf(CLEAR_SCREEN);
    printf(BOLD FCYN TRIS_ASCII_ART_CLIENT NO_BOLD FNRM);
}

void printWelcomeMessageServer()
{
    printHeaderServer();
    printf(CREDITS);
    printf(NO_BOLD FNRM);
}

void printWelcomeMessageClient(char *username)
{
    printHeaderClient();
    printf(CREDITS);
    printf(WELCOME_CLIENT_MESSAGE, username);
}

void printLoadingMessage()
{
    printf(LOADING_MESSAGE FGRN);

#if DEBUG
    printf(MY_PID_MESSAGE, getpid());
#endif
}

void printAndFlush(const char *msg)
{
    printf("%s", msg);
    fflush(stdout);
}

void *loadingSpinner()
{
    while (1)
    {
        printAndFlush("\b|");
        usleep(100000);
        printAndFlush("\b/");
        usleep(100000);
        printAndFlush("\b-");
        usleep(100000);
        printAndFlush("\b\\");
        usleep(100000);
    }

    return NULL;
}

pthread_t startLoadingSpinner()
{
    pthread_t tid;
    pthread_create(&tid, NULL, loadingSpinner, NULL);
    return tid;
}

void stopLoadingSpinner(pthread_t tid)
{
    pthread_cancel(tid);
    printAndFlush("\b \b");
}

int setInput(struct termios *policy)
{
    return tcsetattr(STDOUT_FILENO, TCSANOW, policy);
}

void ignorePreviousInput()
{
    tcflush(STDIN_FILENO, TCIFLUSH);
}

bool initOutputSettings(struct termios *withEcho, struct termios *withoutEcho)
{
    if (tcgetattr(STDOUT_FILENO, withEcho) != 0)
    {
        return false;
    }

    memcpy(withoutEcho, withEcho, sizeof(struct termios));
    withoutEcho->c_lflag &= ~ECHO;

    return true;
}

void printLoadingCompleteMessage()
{
    printf(LOADING_COMPLETE_MESSAGE);
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
    printf(MATRIX_INITIALIZED_MESSAGE);
#endif
}

void printBoard(int *matrix, char playerOneSymbol, char playerTwoSymbol)
{
    int cell;

    printf(MATRIX_TOP_ROW);
    for (int i = 0; i < MATRIX_SIDE_LEN; i++)
    {
        printf("  %d  ", i + 1);

        for (int j = 0; j < MATRIX_SIDE_LEN; j++)
        {
            cell = matrix[i * MATRIX_SIDE_LEN + j];

            switch (cell)
            {
            case 0:
                printf(EMPTY_CELL);
                break;
            case 1:
                printf(PLAYER_ONE_COLOR BOLD " %c" FNRM NO_BOLD, playerOneSymbol);
                break;
            case 2:
                printf(PLAYER_TWO_COLOR BOLD " %c" FNRM NO_BOLD, playerTwoSymbol);
                break;
            }

            if (j < MATRIX_SIDE_LEN - 1)
            {
                printf(VERTICAL_SEPARATOR);
            }
        }
        if (i < MATRIX_SIDE_LEN - 1)
        {
            printf(HORIZONTAL_SEPARATOR);
        }
    }
    printf("\n\n");
}

void printSymbol(char symbol, int playerIndex, char *username)
{
    printf(YOUR_SYMBOL_IS_MESSAGE, username, playerIndex == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, symbol);
}

void printTimeout(int timeout)
{
    printf(timeout == 0 ? INFINITE_TIMEOUT_MESSAGE : TIMEOUT_MESSAGE, timeout);
}

void printError(const char *msg)
{
    printAndFlush(FRED ERROR_CHAR);
    printAndFlush(msg);
    printAndFlush(FNRM);
}

void clearScreenServer()
{
    printHeaderServer();
}

void clearScreenClient()
{
    printHeaderClient();
}

void printSuccess(const char *msg)
{
    printAndFlush(FGRN SUCCESS_CHAR);
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
int recordJoin(tris_game_t *game, int pid, char *username)
{
    int semId = getSemaphores(N_SEM), playerIndex = -1;

    waitSemaphore(semId, PID_LOCK, 1);
    for (int i = 0; i < PID_ARRAY_LEN; i++)
    {
        if (game->pids[i] == 0)
        {
            game->pids[i] = pid;
            playerIndex = i;
            strcpy(game->usernames[i], username);

            break;
        }
    }
    signalSemaphore(semId, PID_LOCK, 1);

    return playerIndex;
}

void setPidAt(int *pidsPointer, int index, int pid)
{
    int semId = getSemaphores(N_SEM);

    waitSemaphore(semId, PID_LOCK, 1);
    pidsPointer[index] = pid;
    signalSemaphore(semId, PID_LOCK, 1);
}

void recordQuit(tris_game_t *game, int index)
{
    int semId = getSemaphores(N_SEM);

    waitSemaphore(semId, PID_LOCK, 1);
    game->pids[index] = 0;
    free(game->usernames[index]);
    signalSemaphore(semId, PID_LOCK, 1);
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