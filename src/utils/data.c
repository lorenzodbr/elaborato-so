#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define DEBUG 1

#define N_ARGS 4
#define MATRIX_SIZE 3

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

#define PROJ_ID 163
#define FTOK_PATH "../.config"

#define CLEAR_SCREEN "\033[H\033[J"
#define LOADING_MESSAGE "\nCaricamento in corso...  \n"
#define INIT_ERROR "Errore durante l'inizializzazione."
#define VERSION "1.0"
#define TRIS_ASCII_ART_TOP "              ______                     \n\
             /\\__  _\\       __           \n\
             \\/_/\\ \\/ _ __ /\\_\\    ____  \n\
                \\ \\ \\/\\`'__\\/\\ \\  / ,__\\ \n\
                 \\ \\ \\ \\ \\/ \\ \\ \\/\\__, `\\ \n\
                  \\ \\_\\ \\_\\  \\ \\_\\/\\____/ "

                   
#define TRIS_ASCII_ART_BOTTOM "\n                   \\/_/\\/_/   \\/_/\\/___/  v" VERSION "\n                                   \n"
#define TRIS_ASCII_ART_SERVER TRIS_ASCII_ART_TOP "Server" TRIS_ASCII_ART_BOTTOM
#define TRIS_ASCII_ART_CLIENT TRIS_ASCII_ART_TOP "Client" TRIS_ASCII_ART_BOTTOM
#define CREDITS "Progetto realizzato da Lorenzo Di Berardino e Filippo Milani\n"
#define USAGE_ERROR "Usage: ./TrisServer <timeout> <playerOneSymbol> <playerTwoSymbol>\n"