/************************************
* VR487434 - Lorenzo Di Berardino
* VR486588 - Filippo Milani
* 09/05/2024
*************************************/

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <limits.h>
#include <stdbool.h>

#include "semaphores/semaphores.h"

typedef struct {
    int row;
    int col;
} move_t;

typedef struct {
    int matrix[MATRIX_SIZE];
    pid_t pids[PID_ARRAY_LEN];
    char usernames[USERNAMES_ARRAY_LEN][USERNAME_MAX_LEN];
    int result;
    int autoplay;
    char symbols[SYMBOLS_ARRAY_LEN];
    int timeout;
} tris_game_t;

void printHeaderServer() {
    printf(CLEAR_SCREEN);
    printf(TRIS_ASCII_ART_SERVER);
}

void printHeaderClient() {
    printf(CLEAR_SCREEN);
    printf(BOLD FCYN TRIS_ASCII_ART_CLIENT NO_BOLD FNRM);
}

void printWelcomeMessageServer() {
    printHeaderServer();
    printf(CREDITS);
    printf(NO_BOLD FNRM);
}

void printWelcomeMessageClient(char* username) {
    printHeaderClient();
    printf(CREDITS);
    printf(WELCOME_CLIENT_MESSAGE, username);
}

void printLoadingMessage() {
    printf(LOADING_MESSAGE FGRN);

#if DEBUG
    printf(MY_PID_MESSAGE, getpid());
#endif
}

void printAndFlush(const char* msg) {
    printf("%s", msg);
    fflush(stdout);
}

void* loadingSpinner() {
    while (1) {
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

void startLoadingSpinner(pthread_t* tid) {
    pthread_create(tid, NULL, loadingSpinner, NULL);
}

void stopLoadingSpinner(pthread_t* tid) {
    if (*tid != 0) {
        pthread_cancel(*tid);
        printAndFlush("\b \b");
        *tid = 0;
    }
}

int digits(int n) {
    if (n < 0)
        return digits((n == INT_MIN) ? INT_MAX : -n);
    if (n < 10)
        return 1;
    return 1 + digits(n / 10);
}

void printSpaces(int n) {
    for (int i = 0; i < n; i++) {
        printf(" ");
    }

    fflush(stdout);
}

void* timeoutPrintHandler(void* timeout) {
    int timeoutValue = *(int*)timeout;
    int originalDigits = digits(timeoutValue) + (timeoutValue % 10 != 0), newDigits;

    while (timeoutValue > 0) {
        newDigits = digits(timeoutValue);
        printf("\x1b[s\r");
        printSpaces(originalDigits - newDigits);
        printf("(%d)\x1b[u", timeoutValue--);
        fflush(stdout);

        sleep(1);
    }

    return NULL;
}

void startTimeoutPrint(pthread_t* tid, int* timeout) {
    pthread_create(tid, NULL, timeoutPrintHandler, timeout);
}

void stopTimeoutPrint(pthread_t tid) {
    if (tid == 0) {
        return;
    }

    pthread_cancel(tid);
}

int setInput(struct termios* policy) {
    return tcsetattr(STDOUT_FILENO, TCSANOW, policy);
}

void ignorePreviousInput() {
    tcflush(STDIN_FILENO, TCIFLUSH);
}

bool initOutputSettings(struct termios* withEcho, struct termios* withoutEcho) {
    if (tcgetattr(STDOUT_FILENO, withEcho) != 0) {
        return false;
    }

    memcpy(withoutEcho, withEcho, sizeof(struct termios));
    withoutEcho->c_lflag &= ~ECHO;

    return true;
}

void printLoadingCompleteMessage() {
    printf(LOADING_COMPLETE_MESSAGE);
}

void initBoard(int* matrix) {
    for (int i = 0; i < MATRIX_SIDE_LEN; i++) {
        for (int j = 0; j < MATRIX_SIDE_LEN; j++) {
            matrix[i * MATRIX_SIDE_LEN + j] = 0;
        }
    }

#if DEBUG
    printf(MATRIX_INITIALIZED_MESSAGE);
#endif
}

void printBoard(int* matrix, char playerOneSymbol, char playerTwoSymbol) {
    int cell;

    printf(MATRIX_TOP_ROW);
    for (int i = 0; i < MATRIX_SIDE_LEN; i++) {
        printf("  %d  ", i + 1);

        for (int j = 0; j < MATRIX_SIDE_LEN; j++) {
            cell = matrix[i * MATRIX_SIDE_LEN + j];

            switch (cell) {
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

            if (j < MATRIX_SIDE_LEN - 1) {
                printf(VERTICAL_SEPARATOR);
            }
        }
        if (i < MATRIX_SIDE_LEN - 1) {
            printf(HORIZONTAL_SEPARATOR);
        }
    }
    printf("\n\n");
}

void printSymbol(char symbol, int playerIndex, char* username) {
    printf(YOUR_SYMBOL_IS_MESSAGE, username, playerIndex == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, symbol);
}

void printTimeout(int timeout) {
    printf(INFO_CHAR);
    printf(timeout == 0 ? INFINITE_TIMEOUT_MESSAGE : TIMEOUT_MESSAGE, timeout);
}

void printError(const char* msg) {
    printAndFlush(FRED ERROR_CHAR);
    printAndFlush(msg);
    printAndFlush(FNRM);
}

void clearScreenServer() {
    printHeaderServer();
}

void clearScreenClient() {
    printHeaderClient();
}

void printSuccess(const char* msg) {
    printAndFlush(FGRN SUCCESS_CHAR);
    printAndFlush(msg);
}

void initPids(int* pidsPointer) {
    for (int i = 0; i < 3; i++) {
        pidsPointer[i] = 0;
    }
}

// set specified pid to the first empty slot (out of 3)
int recordJoin(tris_game_t* game, int pid, char* username, int autoPlay) {
    int semId = getSemaphores(N_SEM), playerIndex = TOO_MANY_PLAYERS_ERROR_CODE;

    waitSemaphore(semId, PID_LOCK, 1);
    for (int i = 1; i < PID_ARRAY_LEN; i++) {
        if (game->pids[i] == 0) {
            int otherPlayerIndex = i == 1 ? 2 : 1;

            if (strcmp(username, game->usernames[otherPlayerIndex]) == 0) {
                playerIndex = SAME_USERNAME_ERROR_CODE;
                break;
            }

            if (autoPlay != NONE) {
                game->autoplay = autoPlay;
            }

            game->pids[i] = pid;
            playerIndex = i;
            strcpy(game->usernames[i], username);

            break;
        }
    }
    signalSemaphore(semId, PID_LOCK, 1);

    return playerIndex;
}

void setPidAt(int* pidsPointer, int index, int pid) {
    int semId = getSemaphores(N_SEM);

    waitSemaphore(semId, PID_LOCK, 1);
    pidsPointer[index] = pid;
    signalSemaphore(semId, PID_LOCK, 1);
}

void recordQuit(tris_game_t* game, int index) {
    int semId = getSemaphores(N_SEM);

    waitSemaphore(semId, PID_LOCK, 1);
    game->pids[index] = 0;
    free(game->usernames[index]);
    signalSemaphore(semId, PID_LOCK, 1);
}

int getPid(int* pidsPointer, int index) {
    return pidsPointer[index];
}

bool isValidMove(int* matrix, char* input, move_t* move) {
    // the input is valid if it is in the format [A-C or a-c],[1-3]

    if (strlen(input) != 2) {
        return false;
    }

    if (((input[0] < 'A' || input[0] > 'C') &&
        (input[0] < 'a' || input[0] > 'c')) ||
        input[1] < '1' || input[1] > '3') {
        return false;
    }

    move->row = input[0] - 'A';

    if (input[0] >= 'a' && input[0] <= 'c') {
        move->row = input[0] - 'a';
    }

    move->col = input[1] - '1';

    if (matrix[move->col * MATRIX_SIDE_LEN + move->row] != 0) {
        return false;
    }

    return true;
}

int isGameEnded(int* matrix) {
    // check rows
    for (int i = 0; i < MATRIX_SIDE_LEN; i++) {
        if (matrix[i * MATRIX_SIDE_LEN] != 0 &&
            matrix[i * MATRIX_SIDE_LEN] == matrix[i * MATRIX_SIDE_LEN + 1] &&
            matrix[i * MATRIX_SIDE_LEN] == matrix[i * MATRIX_SIDE_LEN + 2]) {
            return matrix[i * MATRIX_SIDE_LEN];
        }
    }

    // check columns
    for (int i = 0; i < MATRIX_SIDE_LEN; i++) {
        if (matrix[i] != 0 && matrix[i] == matrix[i + MATRIX_SIDE_LEN] &&
            matrix[i] == matrix[i + 2 * MATRIX_SIDE_LEN]) {
            return matrix[i];
        }
    }

    // check diagonals
    if (matrix[0] != 0 &&
        matrix[0] == matrix[4] &&
        matrix[0] == matrix[8]) {
        return matrix[0];
    }

    if (matrix[2] != 0 &&
        matrix[2] == matrix[4] &&
        matrix[2] == matrix[6]) {
        return matrix[2];
    }

    // check draw
    for (int i = 0; i < MATRIX_SIZE; i++) {
        if (matrix[i] == 0) {
            return NOT_FINISHED;
        }
    }

    return DRAW;
}

int max(int a, int b) {
    return a > b ? a : b;
}

int min(int a, int b) {
    return a < b ? a : b;
}

int minimax(int* gameMatrix, int depth, bool isMaximizing) {
    int result = isGameEnded(gameMatrix);

    if(result == DRAW) return DRAW;

    if (result != NOT_FINISHED) {
        return result == PLAYER_TWO ? 1 : -1;
    }

    if (isMaximizing) {
        int bestVal = INT_MIN;

        for (int i = 0; i < MATRIX_SIZE; i++) {
            if (gameMatrix[i] == 0) {
                gameMatrix[i] = PLAYER_TWO;
                bestVal = max(bestVal, minimax(gameMatrix, depth + 1, false));
                gameMatrix[i] = 0;
            }
        }

        return bestVal;
    } else {
        int bestVal = INT_MAX;

        for (int i = 0; i < MATRIX_SIZE; i++) {
            if (gameMatrix[i] == 0) {
                gameMatrix[i] = PLAYER_ONE;
                bestVal = min(bestVal, minimax(gameMatrix, depth + 1, true));
                gameMatrix[i] = 0;
            }
        }

        return bestVal;
    }
}

//uses minimax to choose the next best move
void chooseNextBestMove(int* gameMatrix, int playerIndex) {
    int bestVal = INT_MIN;
    move_t bestMove;

    for (int i = 0; i < MATRIX_SIZE; i++) {
        if (gameMatrix[i] == 0) {
            gameMatrix[i] = playerIndex;
            int moveVal = minimax(gameMatrix, 0, false);
            gameMatrix[i] = 0;

            if (moveVal > bestVal) {
                bestMove.row = i % MATRIX_SIDE_LEN;
                bestMove.col = i / MATRIX_SIDE_LEN;
                bestVal = moveVal;
            }
        }
    }

    gameMatrix[bestMove.row + bestMove.col * MATRIX_SIDE_LEN] = playerIndex;
}

void chooseRandomMove(int* gameMatrix, int playerIndex) {
    int randomIndex;

    do {
        randomIndex = rand() % MATRIX_SIZE;
    } while (gameMatrix[randomIndex] != 0);

    gameMatrix[randomIndex] = playerIndex;
}


void chooseRandomOrBestMove(int* gameMatrix, int playerIndex, int cycles) {
    if(cycles % 2 == 0)
        chooseRandomMove(gameMatrix, playerIndex);
    else
        chooseNextBestMove(gameMatrix, playerIndex);
}