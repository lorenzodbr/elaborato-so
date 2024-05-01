#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#define DEBUG 0

// Args
#define N_ARGS_SERVER 3
#define N_ARGS_CLIENT 2

// Colors
// Foreground
#define FNRM "\x1B[0m"
#define FRED "\x1B[31m"
#define FGRN "\x1B[32m"
#define FYEL "\x1B[33m"
#define FBLU "\x1B[34m"
#define FMAG "\x1B[35m"
#define FCYN "\x1B[36m"
#define FWHT "\x1B[37m"
#define BOLD "\033[1m"
#define NO_BOLD "\033[22m"

// Background
#define BNRM "\x1B[0m"
#define BGRY "\033[100m"

// Players colors
#define PLAYER_ONE_COLOR FBLU
#define PLAYER_TWO_COLOR FRED

// Chars
#define NEWLINE "\n"
#define ERROR_CHAR " ✗ "
#define SUCCESS_CHAR " ✔ "

// Keys for IPCs
#define MATRIX_ID 163
#define PID_ID 110
#define SEM_ID 213
#define RESULT_ID 313
#define SYMBOLS_ID 413
#define FTOK_PATH ".config"

// Semaphore indexes
#define N_SEM 8
#define WAIT_FOR_PLAYERS 0
#define WAIT_FOR_OPPONENT_READY 1
#define LOCK 2
#define PLAYER_ONE_TURN 3
#define PLAYER_TWO_TURN 4
#define WAIT_FOR_MOVE 5

// PID indexes
#define SERVER 0
#define PLAYER_ONE 1
#define PLAYER_TWO 2

// Game settings
#define INITIAL_TURN PLAYER_ONE

// Game results
#define NOT_FINISHED -1
#define DRAW 0
#define PLAYER_ONE_WIN 1
#define PLAYER_TWO_WIN 2
#define QUIT 3
#define PLAYER_ONE_WIN_FOR_QUIT PLAYER_ONE_WIN + QUIT
#define PLAYER_TWO_WIN_FOR_QUIT PLAYER_TWO_WIN + QUIT

// Sizes
#define MATRIX_SIDE_LEN 3
#define MATRIX_SIZE MATRIX_SIDE_LEN *MATRIX_SIDE_LEN * sizeof(char)
#define PID_SIZE 3 * sizeof(pid_t)
#define RESULT_SIZE sizeof(int)
#define SYMBOLS_SIZE 2 * sizeof(char)
#define INPUT_LEN 2

// Strings
#define CLEAR_SCREEN "\033[H\033[J"
#define LOADING_MESSAGE "Caricamento in corso...  \n"
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

// Messages
#define WAITING_FOR_PLAYERS_MESSAGE "\n[!] In attesa di giocatori... "
#define SERVER_QUIT_MESSAGE FRED "\n\nIl server ha terminato la partita.\n"
#define CTRLC_AGAIN_TO_QUIT_MESSAGE FYEL "\n\nPremi CTRL+C di nuovo per uscire.\n"
#define WINS_PLAYER_MESSAGE FGRN "Vince il giocatore %d (con PID = %d).\n"
#define WAITING_FOR_MOVE_SERVER_MESSAGE "In attesa della mossa del %sgiocatore %d%s (con PID = %d)...\n"
#define MOVE_RECEIVED_SERVER_MESSAGE "Mossa ricevuta dal giocatore %d (con PID = %d).\n\n"
#define A_PLAYER_JOINED_SERVER_MESSAGE "\nUn giocatore con PID = %d (%c) è entrato in partita.\n"
#define ANOTHER_PLAYER_JOINED_SERVER_MESSAGE "Un altro giocatore con PID = %d (%c) è entrato in partita.\n\nPronti per cominciare. "
#define A_PLAYER_QUIT_SERVER_MESSAGE "\nIl giocatore %d (con PID = %d) ha abbandonato la partita.\n\n"
#define STARTS_PLAYER_MESSAGE "Inizia il giocatore %d (con PID = %d).\n\n"
#define DRAW_MESSAGE FYEL "Pareggio.\n"
#define CLOSING_MESSAGE "\nChiusura in corso...\n"
#define YOU_WON_FOR_QUIT_MESSAGE FGRN "\n\nHai vinto per abbandono dell'altro giocatore!\n"
#define YOU_LOST_MESSAGE FRED "Hai perso!\n"
#define YOU_WON_MESSAGE FGRN "Hai vinto!\n"
#define INPUT_A_MOVE_MESSAGE "Inserisci la mossa (LetteraNumero): "
#define WAITING_FOR_OPPONENT_MESSAGE FNRM "In attesa dell'avversario... "
#define OPPONENT_READY_MESSAGE "Avversario pronto!"
#define OPPONENT_TURN_MESSAGE "(Turno dell'avversario) "
#define YOUR_SYMBOL_IS_MESSAGE FYEL "%s" FNRM ", il tuo simbolo è " BOLD "%s%c%s\n" NO_BOLD
#define LOADING_COMPLETE_MESSAGE FNRM "Caricamento completato.\n"

// Errors
#define USAGE_ERROR_SERVER "Uso: ./TrisServer <timeout> <playerOneSymbol> <playerTwoSymbol>"
#define USAGE_ERROR_CLIENT "Uso: ./TrisClient <username>"
#define TOO_MANY_PLAYERS_ERROR "\nTroppi giocatori connessi. Riprova più tardi.\n"
#define INITIALIZATION_ERROR "Errore durante l'inizializzazione."
#define SYMBOLS_LENGTH_ERROR "I simboli dei giocatori devono essere di un solo carattere."
#define INVALID_MOVE_ERROR "Mossa non valida. Riprova: "
#define NO_SERVER_FOUND_ERROR "Nessun server trovato. Esegui TrisServer prima di eseguire TrisClient.\n"