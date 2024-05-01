#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>

#include "utils/data.h"
#include "utils/globals.h"
#include "utils/shared_memory/shared_memory.h"

void init();
void notifyPlayerReady();
void notifyMove();
void initSharedMemory();
void initSemaphores();
void askForInput();
void waitForOpponent();
void initSignals();
void exitHandler(int sig);
void quitHandler(int sig);
void checkResults(int sig);
void serverQuitHandler(int sig);
void waitForMove();
void waitForResponse();
void printMoveScreen();
void initTimeout();
void resetTimeout();
void timeoutHandler(int sig);
void showInput();
void initTerminalSettings();

// Shared memory

tris_game_t *game;
int gameId;

// Semaphores
int semId;

// State variables
bool firstCTRLCPressed = false;
int playerIndex = -1;
char *username;

// Terminal settings
struct termios withEcho, withoutEcho;
bool outputCustomizable = true;

int main(int argc, char *argv[])
{
    if (argc != N_ARGS_CLIENT + 1 && argc != N_ARGS_CLIENT)
    {
        errExit(USAGE_ERROR_CLIENT);
    }

    username = argv[1];

    init();
    notifyPlayerReady();
    waitForOpponent();

    if (playerIndex != INITIAL_TURN)
    {
        printMoveScreen();
        printAndFlush(OPPONENT_TURN_MESSAGE);
    }

    do
    {
        waitForMove();

        // Prints before the move
        printMoveScreen();

        askForInput();
        notifyMove();

        // Prints after the move
        printMoveScreen();
        printAndFlush(OPPONENT_TURN_MESSAGE);

        setInput(&withoutEcho);
    } while (1);

    return EXIT_SUCCESS;
}

void init()
{
    printWelcomeMessageClient(username);
    printLoadingMessage();

    initSemaphores();
    initSharedMemory();
    initSignals();
    initTerminalSettings();

    printLoadingCompleteMessage();
}

void initSharedMemory()
{
    gameId = getSharedMemory(GAME_SIZE, GAME_ID);
    if (gameId < 0)
    {
        printError(NO_SERVER_FOUND_ERROR);
        exit(EXIT_FAILURE);
    }

    game = attachSharedMemory(gameId);


#if DEBUG
    printf(SHARED_MEMORY_OBTAINED_SUCCESS, gameId);
#endif

    playerIndex = setPid(game->pids, getpid());

    if (playerIndex == -1)
    {
        printError(TOO_MANY_PLAYERS_ERROR);
        exit(EXIT_FAILURE);
    }

#if DEBUG
    printf(SERVER_FOUND_SUCCESS, game->pids[SERVER]);
#endif
}

void initSemaphores()
{
    semId = getSemaphores(N_SEM);
}

void initSignals()
{
    sigset_t set;
    sigfillset(&set);
    sigdelset(&set, SIGINT);
    sigdelset(&set, SIGUSR1);
    sigdelset(&set, SIGUSR2);
    sigdelset(&set, SIGHUP);
    sigdelset(&set, SIGTERM);
    sigdelset(&set, SIGALRM);
    sigprocmask(SIG_SETMASK, &set, NULL);

    if (signal(SIGINT, exitHandler) == SIG_ERR ||
        signal(SIGUSR1, serverQuitHandler) == SIG_ERR ||
        signal(SIGUSR2, checkResults) == SIG_ERR ||
        signal(SIGHUP, quitHandler) == SIG_ERR ||
        signal(SIGTERM, exitHandler) == SIG_ERR)
    {
        printError(INITIALIZATION_ERROR);
        exit(EXIT_FAILURE);
    }
}

void initTerminalSettings()
{
    if ((outputCustomizable = initOutputSettings(&withEcho, &withoutEcho)))
    {
        setInput(&withoutEcho);
    }
    
    if(atexit(showInput))
    {
        printError(INITIALIZATION_ERROR);
        exit(EXIT_FAILURE);
    }
}

void showInput()
{
    setInput(&withEcho);
}

void notifyPlayerReady()
{
    signalSemaphore(semId, WAIT_FOR_PLAYERS, 1);
}

void notifyMove()
{
    signalSemaphore(semId, WAIT_FOR_MOVE, 1);
}

void waitForOpponent()
{
    printAndFlush(WAITING_FOR_OPPONENT_MESSAGE);

    do
    {
        errno = 0;
        waitSemaphore(semId, WAIT_FOR_OPPONENT_READY, 1);
    } while (errno == EINTR);

    printAndFlush(OPPONENT_READY_MESSAGE);
}

void waitForMove()
{
    do
    {
        errno = 0;
        waitSemaphore(semId, PLAYER_ONE_TURN + playerIndex - 1, 1);
    } while (errno == EINTR);

    ignorePreviousInput();
}

void waitForResponse()
{
    do
    {
        errno = 0;
        waitSemaphore(semId, PLAYER_ONE_TURN + playerIndex - 1, 1);
    } while (errno == EINTR);
}

void timeoutHandler(int sig)
{
    printf(TIMEOUT_LOSS_MESSAGE);
    quitHandler(sig);
    exit(EXIT_FAILURE);
}

void initTimeout()
{
    if (game->timeout == 0)
    {
        return;
    }

    alarm(game->timeout);
    signal(SIGALRM, timeoutHandler);
}

void resetTimeout()
{
    if (game->timeout == 0)
    {
        return;
    }

    alarm(0);
}

void askForInput()
{
    char input[MOVE_INPUT_LEN];

    showInput(withoutEcho);
    initTimeout();

    printf(INPUT_A_MOVE_MESSAGE);
    scanf("%s", input);

    move_t move;

    while (!isValidMove(game->matrix, input, &move))
    {
        firstCTRLCPressed = false; // reset firstCTRLCPressed if something else is inserted

        printError(INVALID_MOVE_ERROR);
        scanf("%s", input);
    }

    game->matrix[move.row + move.col * MATRIX_SIDE_LEN] = playerIndex;

    resetTimeout();
}

void printMoveScreen()
{
    clearScreenClient();
    printSymbol(game->symbols[playerIndex - 1], playerIndex, username);
    printTimeout(game->timeout);
    printBoard(game->matrix, game->symbols[0], game->symbols[1]);
}

void checkResults(int sig)
{
    printMoveScreen();

    if (game->result == DRAW)
    {
        printf(DRAW_MESSAGE);
    }
    else if (game->result == QUIT)
    {
        printf(YOU_WON_FOR_QUIT_MESSAGE);
    }
    else if (game->result != playerIndex)
    {
        printf(YOU_LOST_MESSAGE);
    }
    else
    {
        printf(YOU_WON_MESSAGE);
    }

    exit(EXIT_SUCCESS);
}

void exitHandler(int sig)
{
    if (firstCTRLCPressed)
    {
        kill(game->pids[SERVER], playerIndex == 1 ? SIGUSR1 : SIGUSR2);
        printf(CLOSING_MESSAGE);
        exit(EXIT_SUCCESS);
    }
    firstCTRLCPressed = true;
    printf(CTRLC_AGAIN_TO_QUIT_MESSAGE);
}

void quitHandler(int sig)
{
    kill(game->pids[SERVER], playerIndex == 1 ? SIGUSR1 : SIGUSR2);
}

void serverQuitHandler(int sig)
{
    printf(SERVER_QUIT_MESSAGE);
    exit(EXIT_FAILURE);
}