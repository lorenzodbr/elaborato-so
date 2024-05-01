#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

// ----------------- DEBUG ------------------

#define DEBUG 0

// Args
#define N_ARGS_SERVER 3
#define N_ARGS_CLIENT 2

// ----------------- COLORS ------------------

#define CLEAR_SCREEN "\033[H\033[J"

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

// ------------- APPLICATION STRINGS --------------

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
#define CREDITS FCYN BOLD "Progetto realizzato da Lorenzo Di Berardino e Filippo Milani\n\n" NO_BOLD FNRM

#define NEWLINE "\n"
#define ERROR_CHAR " [✗] "
#define SUCCESS_CHAR " [✔] "
#define INFO_CHAR " [i] "
#define WARNING_CHAR " [!] "
#define EMPTY_CELL "  "
#define VERTICAL_SEPARATOR " │"
#define HORIZONTAL_SEPARATOR "\n    ───┼───┼───\n"
#define MATRIX_TOP_ROW "\n     A   B   C\n\n"

// ------------------ IPCS --------------------

// Permissions
#define PERM 0640

// Keys
#define MATRIX_ID 163
#define PID_ID 110
#define SEM_ID 213
#define RESULT_ID 313
#define SYMBOLS_ID 413
#define TIMEOUT_ID 513
#define FTOK_PATH ".config"

// Semaphore indexes
#define N_SEM 8
#define WAIT_FOR_PLAYERS 0
#define WAIT_FOR_OPPONENT_READY 1
#define LOCK 2
#define PLAYER_ONE_TURN 3
#define PLAYER_TWO_TURN 4
#define WAIT_FOR_MOVE 5

// Sizes
#define MATRIX_SIDE_LEN 3
#define MATRIX_SIZE MATRIX_SIDE_LEN *MATRIX_SIDE_LEN * sizeof(char)
#define PID_SIZE 3 * sizeof(pid_t)
#define RESULT_SIZE sizeof(int)
#define SYMBOLS_SIZE 2 * sizeof(char)
#define INPUT_LEN 2

// ----------------- GAME --------------------

// Settings
#define INITIAL_TURN PLAYER_ONE
#define MINIMUM_TIMEOUT 10

// Results
#define NOT_FINISHED -1
#define DRAW 0
#define PLAYER_ONE_WIN 1
#define PLAYER_TWO_WIN 2
#define QUIT 3
#define PLAYER_ONE_WIN_FOR_QUIT PLAYER_ONE_WIN + QUIT
#define PLAYER_TWO_WIN_FOR_QUIT PLAYER_TWO_WIN + QUIT

// Roles
#define SERVER 0
#define PLAYER_ONE 1
#define PLAYER_TWO 2

// ----------------- MESSAGES ----------------

#define GAME_SETTINGS_MESSAGE INFO_CHAR "Impostazioni:\n"
#define TIMEOUT_SETTINGS_MESSAGE "     ─ Timeout: %d secondi\n"
#define INFINITE_TIMEOUT_SETTINGS_MESSAGE "     ─ Timeout: ∞\n"
#define PLAYER_ONE_SYMBOL_SETTINGS_MESSAGE "     ─ Simbolo " PLAYER_ONE_COLOR "giocatore 1" FNRM ": %c\n"
#define PLAYER_TWO_SYMBOL_SETTINGS_MESSAGE "     ─ Simbolo " PLAYER_TWO_COLOR "giocatore 2" FNRM ": %c\n"
#define LOADING_MESSAGE INFO_CHAR "Caricamento in corso...  \n"
#define WELCOME_CLIENT_MESSAGE FNRM NO_BOLD "\nBenvenuto, " FYEL "%s!" FNRM "\n\n"
#define WAITING_FOR_PLAYERS_MESSAGE "\n" WARNING_CHAR "In attesa di giocatori... "
#define SERVER_QUIT_MESSAGE FRED "\n\nIl server ha terminato la partita.\n"
#define MY_PID_MESSAGE SUCCESS_CHAR "PID = %d\n"
#define CTRLC_AGAIN_TO_QUIT_MESSAGE FYEL "\n\nPremi CTRL+C di nuovo per uscire." FNRM "\n"
#define WINS_PLAYER_MESSAGE FGRN SUCCESS_CHAR "Vince il giocatore %d (con PID = %d).\n"
#define WAITING_FOR_MOVE_SERVER_MESSAGE "In attesa della mossa del %sgiocatore %d%s (con PID = %d)...\n"
#define MOVE_RECEIVED_SERVER_MESSAGE "Mossa ricevuta dal %sgiocatore %d%s (con PID = %d).\n\n"
#define A_PLAYER_JOINED_SERVER_MESSAGE "\nUn giocatore con PID = %d (%c) è entrato in partita. "
#define ANOTHER_PLAYER_JOINED_SERVER_MESSAGE "Un altro giocatore con PID = %d (%c) è entrato in partita. \n\n" INFO_CHAR " Pronti per cominciare. "
#define A_PLAYER_QUIT_SERVER_MESSAGE "\nIl giocatore %d (con PID = %d) ha abbandonato la partita.\n\n"
#define STARTS_PLAYER_MESSAGE "Inizia il giocatore %d (con PID = %d).\n\n"
#define DRAW_MESSAGE FYEL WARNING_CHAR "Pareggio.\n"
#define CLOSING_MESSAGE FYEL "\nChiusura in corso...\n"
#define YOU_WON_FOR_QUIT_MESSAGE FGRN "Hai vinto per abbandono dell'altro giocatore!\n"
#define YOU_LOST_MESSAGE FRED ERROR_CHAR "Hai perso!\n"
#define YOU_WON_MESSAGE FGRN SUCCESS_CHAR "Hai vinto!\n"
#define INPUT_A_MOVE_MESSAGE "Inserisci la mossa (LetteraNumero): "
#define WAITING_FOR_OPPONENT_MESSAGE FNRM WARNING_CHAR "In attesa dell'avversario... "
#define OPPONENT_READY_MESSAGE "Avversario pronto!"
#define OPPONENT_TURN_MESSAGE "(Turno dell'avversario) "
#define YOUR_SYMBOL_IS_MESSAGE INFO_CHAR FYEL "%s" FNRM ", il tuo simbolo è " BOLD "%s%c%s\n" NO_BOLD
#define LOADING_COMPLETE_MESSAGE FNRM SUCCESS_CHAR "Caricamento completato.\n\n"
#define TIMEOUT_MESSAGE INFO_CHAR "Timeout: %d secondi.\n"
#define INFINITE_TIMEOUT_MESSAGE INFO_CHAR "Timeout: ∞\n"
#define TIMEOUT_LOSS_MESSAGE FRED "\n\nIl tempo è scaduto. Hai perso per inattività.\n"
#define MATRIX_INITIALIZED_MESSAGE FGRN SUCCESS_CHAR "Matrice inizializzata.\n" FNRM

// Successes
#define SERVER_FOUND_SUCCESS FGRN SUCCESS_CHAR "Trovato TrisServer con PID = %d\n" FNRM

#define SEMAPHORE_OBTAINED_SUCCESS FGRN SUCCESS_CHAR "Semafori ottenuti.\n" FNRM
#define SEMAPHORES_INITIALIZED_SUCCESS FGRN SUCCESS_CHAR "Semafori inizializzati.\n" FNRM
#define SEMAPHORES_DISPOSED_SUCCESS FGRN SUCCESS_CHAR "Semafori deallocati.\n" FNRM
#define SHARED_MEMORY_OBTAINED_SUCCESS FGRN SUCCESS_CHAR "Memoria condivisa ottenuta (ID: %d).\n" FNRM
#define SHARED_MEMORY_INITIALIZED_SUCCESS FGRN SUCCESS_CHAR "Memoria condivisa deallocata.\n" FNRM
#define SHARED_MEMORY_ATTACHED_SUCCESS FGRN SUCCESS_CHAR "Memoria condivisa agganciata (@ %p).\n" FNRM

// ----------------- ERRORS ------------------

// General errors
#define USAGE_ERROR_SERVER "Uso: ./TrisServer <timeout> <playerOneSymbol> <playerTwoSymbol>"
#define USAGE_ERROR_CLIENT "Uso: ./TrisClient <username>"
#define TOO_MANY_PLAYERS_ERROR "\nTroppi giocatori connessi. Riprova più tardi.\n"
#define INITIALIZATION_ERROR "Errore durante l'inizializzazione."
#define INVALID_MOVE_ERROR "Mossa non valida. Riprova: "
#define NO_SERVER_FOUND_ERROR "Nessun server trovato. Esegui TrisServer prima di eseguire TrisClient.\n"

// Args errors
#define TIMEOUT_INVALID_CHAR_ERROR "Il valore specificato per il timeout non è valido."
#define TIMEOUT_TOO_LOW_ERROR "Il minimo timeout ammesso è di 10 secondi"
#define SYMBOLS_LENGTH_ERROR "I simboli dei giocatori devono essere di un solo carattere."
#define SYMBOLS_EQUAL_ERROR "I simboli dei giocatori devono essere diversi."

// Semaphore errors
#define SEMAPHORE_ALLOCATION_ERROR "Errore durante la creazione dei semafori."
#define SEMAPHORE_INITIALIZATION_ERROR "Errore durante l'inizializzazione dei semafori."
#define SEMAPHORE_DEALLOCATION_ERROR "Errore durante la deallocazione dei semafori."
#define SEMAPHORE_WAITING_ERROR "Errore durante l'attesa del semaforo."
#define SEMAPHORE_SIGNALING_ERROR "Errore durante il rilascio del semaforo."

// Shared memory errors
#define SHARED_MEMORY_ALLOCATION_ERROR "Errore durante la creazione della memoria condivisa."
#define SHARED_MEMORY_ATTACH_ERROR "Errore durante l'aggancio della memoria condivisa."
#define SHARED_MEMORY_DEALLOCATION_ERROR "Errore durante la deallocazione della memoria condivisa."

// Fork errors
#define FORK_ERROR "Errore durante la creazione di un processo figlio."
