#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define N_ARGS 4
#define NEWLINE "\n"

#define KNRM "\x1B[0m"
#define KRED "\x1B[31m"
#define KGRN "\x1B[32m"
#define KYEL "\x1B[33m"
#define KBLU "\x1B[34m"
#define KMAG "\x1B[35m"
#define KCYN "\x1B[36m"
#define KWHT "\x1B[37m"

#define CLEAR_SCREEN "\033[H\033[J"
#define LOADING_MESSAGE "Caricamento in corso...  "
#define INIT_ERROR "Errore durante l'inizializzazione."
#define VERSION "1.0"
#define TRIS_ASCII_ART "              ______                     \n\
             /\\__  _\\       __           \n\
             \\/_/\\ \\/ _ __ /\\_\\    ____  \n\
                \\ \\ \\/\\`'__\\/\\ \\  / ,__\\ \n\
                 \\ \\ \\ \\ \\/ \\ \\ \\/\\__, `\\ \n\
                  \\ \\_\\ \\_\\  \\ \\_\\/\\____/ \n\
                   \\/_/\\/_/   \\/_/\\/___/  v" VERSION "\n\
                                   "
#define CREDITS "Progetto realizzato da: Lorenzo Di Berardino e Filippo Milani"
#define USAGE_ERROR "Usage: ./TrisServer <timeout> <playerOneSymbol> <playerTwoSymbol>"