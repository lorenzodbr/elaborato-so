#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define DEBUG 1

#define N_ARGS_SERVER 4
#define MATRIX_SIDE_LEN 3

#define NEWLINE "\n"
#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"
#define BOLD "\033[1m"
#define NO_BOLD "\033[22m"

#define ERROR_CHAR " ✗ "
#define SUCCESS_CHAR " ✔ "

#define MATRIX_ID 163
#define PID_ID 011
#define SEM_ID 213

#define FTOK_PATH ".config"

#define N_SEM 3
#define WAIT_FOR_PLAYERS 0
#define WAIT_FOR_OPPONENT 1
#define LOCK 2

#define SERVER 0
#define PLAYER_ONE 1
#define PLAYER_TWO 2

#define MATRIX_SIZE MATRIX_SIDE_LEN *MATRIX_SIDE_LEN * sizeof(char)
#define PID_SIZE 3 * sizeof(pid_t)

#define CLEAR_SCREEN "\033[H\033[J"
#define LOADING_MESSAGE "\nCaricamento in corso...  \n"
#define INIT_ERROR "Errore durante l'inizializzazione."
#define VERSION "1.0"
#define TRIS_ASCII_ART_TOP "              ______                     \n\
             /\\__  _\\       __           \n\
             \\/_/\\ \\/ _  __/\\_\\    ____  \n\
                \\ \\ \\/\\`'__\\/\\ \\  / ,__\\ \n\
                 \\ \\ \\ \\ \\/ \\ \\ \\/\\__, `\\ \n\
                  \\ \\_\\ \\_\\  \\ \\_\\/\\____/ "

#define TRIS_ASCII_ART_BOTTOM "\n                   \\/_/\\/_/   \\/_/\\/___/  v" VERSION "\n                                   \n"
#define TRIS_ASCII_ART_SERVER TRIS_ASCII_ART_TOP "Server" TRIS_ASCII_ART_BOTTOM
#define TRIS_ASCII_ART_CLIENT TRIS_ASCII_ART_TOP "Client" TRIS_ASCII_ART_BOTTOM
#define CREDITS "Progetto realizzato da Lorenzo Di Berardino e Filippo Milani\n"
#define USAGE_ERROR "Usage: ./TrisServer <timeout> <playerOneSymbol> <playerTwoSymbol>\n"