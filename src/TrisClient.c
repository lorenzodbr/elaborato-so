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

// Shared memory
int *matrix;
int matId;

int *pids;
pid_t pidId;

int *result;
int resId;

char *symbols;
int symId;

// Semaphores
int semId;

// State variables
bool firstCTRLCPressed = false;
int playerIndex = -1;
char *username;

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
        clearScreen();
        printBoard(matrix, symbols[0], symbols[1]);
        printAndFlush("\n(Turno dell'avversario) ");
    }

    do
    {
        waitForMove();
        clearScreen();
        printBoard(matrix, symbols[0], symbols[1]);
        askForInput();
        notifyMove();
        clearScreen();
        printBoard(matrix, symbols[0], symbols[1]);
        printAndFlush("\n(Turno dell'avversario) ");
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
}

void initSharedMemory()
{
    matId = getSharedMemory(MATRIX_SIZE, MATRIX_ID);

    if (matId < 0)
    {
        printf(FRED "Nessun server trovato. Esegui TrisServer prima di eseguire TrisClient.\n");
        exit(EXIT_FAILURE);
    }

    matrix = (int *)attachSharedMemory(matId);

    pidId = getSharedMemory(PID_SIZE, PID_ID);

    if (pidId < 0)
    {
        printf(FRED "Nessun server trovato. Esegui TrisServer prima di eseguire TrisClient.\n");
        exit(EXIT_FAILURE);
    }

    pids = (pid_t *)attachSharedMemory(pidId);

    playerIndex = setPid(pids, getpid());

    if (playerIndex == -1)
    {
        printf(FRED "\nTroppi giocatori connessi. Riprova più tardi.\n");
        exit(EXIT_FAILURE);
    }

    symId = getSharedMemory(SYMBOLS_SIZE, SYMBOLS_ID);
    symbols = (char *)attachSharedMemory(symId);

    printf(FNRM "\nTu sei il %sgiocatore %d%s. Hai il simbolo %s%c\n" FNRM, FYEL, playerIndex, FNRM, playerIndex == PLAYER_ONE ? PLAYER_ONE_COLOR : PLAYER_TWO_COLOR, symbols[playerIndex - 1]);

    resId = getSharedMemory(RESULT_SIZE, RESULT_ID);
    result = (int *)attachSharedMemory(resId);

#if DEBUG
    printf(FGRN SUCCESS_CHAR "Trovato TrisServer con PID = %d\n" FNRM, *pids);
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
    sigprocmask(SIG_SETMASK, &set, NULL);

    if (signal(SIGINT, exitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGTERM, exitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGHUP, quitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGUSR1, serverQuitHandler) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGUSR2, checkResults) == SIG_ERR)
    {
        perror("signal");
        exit(EXIT_FAILURE);
    }
}

void notifyPlayerReady()
{
    signalSemaphore(semId, WAIT_FOR_PLAYERS, 1);
}

void notifyMove()
{
    signalSemaphore(semId, WAIT_FOR_MOVE, 1);
    signalSemaphore(semId, PLAYER_TWO_TURN - playerIndex + 1, 1);
}

void waitForOpponent()
{
    printf(FNRM "In attesa dell'avversario...\n");
    waitSemaphore(semId, WAIT_FOR_OPPONENT_READY, 1);
    printf("Avversario pronto!");
}

void waitForMove()
{
    do
    {
        errno = 0;
        waitSemaphore(semId, PLAYER_ONE_TURN + playerIndex - 1, 1);
    } while (errno == EINTR);

    tcflush(STDIN_FILENO, TCIFLUSH);
}

void askForInput()
{
    char input[50];

    printf("Inserisci una mossa (LetteraNumero): ");
    scanf("%s", input);

    move_t move;

    while (!isValidMove(matrix, input, &move))
    {
        firstCTRLCPressed = false; // reset firstCTRLCPressed if something else is inserted

        printf(FRED "Mossa non valida. Riprova: " FNRM);
        scanf("%s", input);
    }

    matrix[move.row + move.col * MATRIX_SIDE_LEN] = playerIndex;
}

void checkResults(int sig)
{
    if (*result < 3)
    {
        if (*result != playerIndex)
        {
            printf(FRED "\nHai perso!\n");
        }
        else
        {
            printf(FGRN "\nHai vinto!\n");
        }
    }
    else if (*result == DRAW)
    {
        printf(FYEL "\nPareggio!\n");
    }
    else if (*result == QUIT)
    {
        printf(FGRN "\n\nHai vinto per abbandono dell'altro giocatore!\n");
    }

    exit(EXIT_SUCCESS);
}

void exitHandler(int sig)
{
    if (firstCTRLCPressed)
    {
        kill(pids[SERVER], playerIndex == 1 ? SIGUSR1 : SIGUSR2);
        printf("\nChiusura in corso...\n");
        exit(EXIT_SUCCESS);
    }
    firstCTRLCPressed = true;
    printf("\nPremi CTRL+C di nuovo per uscire.\n");
}

void quitHandler(int sig)
{
    kill(pids[SERVER], playerIndex == 1 ? SIGUSR1 : SIGUSR2);
}

void serverQuitHandler(int sig)
{
    printf(FRED "\nIl server ha terminato la partita.\n");
    exit(EXIT_FAILURE);
}