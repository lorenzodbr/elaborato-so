/************************************
* VR487434 - Lorenzo Di Berardino
* VR486588 - Filippo Milani
* 09/05/2024
*************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

// ----------------- DEBUG ------------------

#define DEBUG 0

// Args
#define N_ARGS_SERVER 3
#define N_ARGS_CLIENT 2

// ----------------- COLORS -----------------

#define CLEAR_SCREEN "\033[H\033[J"
#define HIDE_CARET "\33[?25l"
#define SHOW_CARET "\33[?25h"

// Foreground
#define FNRM "\x1B[0m"
#define FRED "\x1B[31m"
#define FGRN "\x1B[32m"
#define FORNG "\x1B[33m"
#define FYEL "\033[1;93m"
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

#define VERSION "1.2"
#define TRIS_ASCII_ART_TOP "              ______                     \n\
             /\\__  _\\       __           \n\
             \\/_/\\ \\/ _  __/\\_\\    ____  \n\
                \\ \\ \\/\\`'__\\/\\ \\  / ,__\\ \n\
                 \\ \\ \\ \\ \\/ \\ \\ \\/\\__, `\\ \n\
                  \\ \\_\\ \\_\\  \\ \\_\\/\\____/ "

#define TRIS_ASCII_ART_BOTTOM "\n                   \\/_/\\/_/   \\/_/\\/___/  v" VERSION "\n                                   \n"
#define TRIS_ASCII_ART_SERVER BOLD FCYN TRIS_ASCII_ART_TOP "Server" TRIS_ASCII_ART_BOTTOM NO_BOLD FNRM
#define TRIS_ASCII_ART_CLIENT BOLD FCYN TRIS_ASCII_ART_TOP "Client" TRIS_ASCII_ART_BOTTOM NO_BOLD FNRM
#define CREDITS FCYN BOLD " Progetto realizzato da Lorenzo Di Berardino e Filippo Milani\n\n" NO_BOLD FNRM

#define NEWLINE "\n"
#define ERROR_CHAR BOLD FRED " [✗] " FNRM NO_BOLD
#define SUCCESS_CHAR BOLD FGRN " [✔] " FNRM NO_BOLD
#define INFO_CHAR BOLD FCYN " [i] " FNRM NO_BOLD
#define WARNING_CHAR BOLD FYEL " [!] " FNRM NO_BOLD
#define EMPTY_CELL "  "
#define VERTICAL_SEPARATOR " │"
#define HORIZONTAL_SEPARATOR "\n     ───┼───┼───\n"
#define MATRIX_TOP_ROW "\n      A   B   C\n\n"
#define EASY_AI_CHAR "*"
#define MEDIUM_AI_CHAR "**"
#define HARD_AI_CHAR "***"

// ------------------ IPCS --------------------

// Permissions
#define PERM 0600

// Keys
#define MATRIX_ID 163
#define PID_ID 110
#define SEM_ID 213
#define RESULT_ID 313
#define SYMBOLS_ID 413
#define TIMEOUT_ID 513
#define GAME_ID 613
#define FTOK_PATH ".config"

// Semaphore indexes
#define N_SEM 6
#define WAIT_FOR_PLAYERS 0
#define WAIT_FOR_OPPONENT_READY 1
#define PID_LOCK 2
#define PLAYER_ONE_TURN 3
#define PLAYER_TWO_TURN 4
#define WAIT_FOR_MOVE 5

// Sizes
#define MATRIX_SIDE_LEN 3
#define MATRIX_SIZE (MATRIX_SIDE_LEN * MATRIX_SIDE_LEN)
#define GAME_SIZE sizeof(tris_game_t)
#define MOVE_INPUT_LEN 20
#define PID_ARRAY_LEN 3
#define USERNAMES_ARRAY_LEN 3
#define USERNAME_MAX_LEN 30
#define USERNAME_MIN_LEN 2
#define SYMBOLS_ARRAY_LEN 2

// ----------------- GAME --------------------

// Settings
#define INITIAL_TURN PLAYER_ONE
#define MINIMUM_TIMEOUT 5

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

// AI difficulty
#define NONE 0
#define EASY 1
#define MEDIUM 2
#define HARD 3

// ----------------- MESSAGES ----------------

// General messages
#define GAME_SETTINGS_MESSAGE FCYN INFO_CHAR "Impostazioni:\n" FNRM
#define TIMEOUT_MESSAGE "Timeout: %d secondi\n"
#define INFINITE_TIMEOUT_MESSAGE "Timeout: ∞\n"
#define TIMEOUT_SETTINGS_MESSAGE "     ─ " TIMEOUT_MESSAGE
#define INFINITE_TIMEOUT_SETTINGS_MESSAGE "     ─ " INFINITE_TIMEOUT_MESSAGE
#define PLAYER_ONE_SYMBOL_SETTINGS_MESSAGE "     ─ Simbolo " PLAYER_ONE_COLOR "giocatore 1" FNRM ": %c\n"
#define PLAYER_TWO_SYMBOL_SETTINGS_MESSAGE "     ─ Simbolo " PLAYER_TWO_COLOR "giocatore 2" FNRM ": %c\n"
#define LOADING_MESSAGE INFO_CHAR "Caricamento in corso...  \n"
#define LOADING_COMPLETE_MESSAGE SUCCESS_CHAR "Caricamento completato!\n\n" FNRM
#define WELCOME_CLIENT_MESSAGE FNRM NO_BOLD "\nBenvenuto, " FORNG "%s!" FNRM "\n\n"
#define READY_TO_START_MESSAGE "\n\n" INFO_CHAR "Pronti per cominciare" FNRM
#define FINAL_STATE_MESSAGE "\n" INFO_CHAR "Stato finale della partita:\n"
#define CLOSING_MESSAGE "\n" WARNING_CHAR "Chiusura in corso...\n"

// Debug messages
#define MY_PID_MESSAGE SUCCESS_CHAR "PID = %d\n"
#define WITH_PID_MESSAGE "(con PID = %d)"

// Game messages
#define CTRLC_AGAIN_TO_QUIT_MESSAGE "\n\n" WARNING_CHAR "Premi CTRL+C di nuovo per uscire" FNRM
#define THIS_WAY_YOU_WILL_LOSE_MESSAGE " (in questo modo perderai la partita)"
#define WAITING_FOR_PLAYERS_MESSAGE "\n" WARNING_CHAR "In attesa di giocatori...  "
#define WAITING_FOR_OPPONENT_MESSAGE FNRM WARNING_CHAR "In attesa dell'avversario...  "
#define SERVER_QUIT_MESSAGE "\n\n" ERROR_CHAR "Il server ha terminato la partita\n"
#define WINS_PLAYER_MESSAGE INFO_CHAR "Vince il %sgiocatore %d" FNRM " (" FORNG "%s" FNRM ")\n"
#define INPUT_A_MOVE_MESSAGE " Inserisci la mossa (LetteraNumero): "
#define WAITING_FOR_MOVE_SERVER_MESSAGE WARNING_CHAR "In attesa della mossa del %sgiocatore %d" FNRM " (" FORNG "%s" FNRM ")... "
#define MOVE_RECEIVED_SERVER_MESSAGE "\n" INFO_CHAR "Mossa ricevuta dal %sgiocatore %d" FNRM " (" FORNG "%s" FNRM ")\n"
#define A_PLAYER_JOINED_SERVER_MESSAGE "\n" INFO_CHAR "Un giocatore " FORNG "%s" FNRM " è entrato in partita "
#define ANOTHER_PLAYER_JOINED_SERVER_MESSAGE INFO_CHAR "Un altro giocatore " FORNG "%s" FNRM " è entrato in partita "
#define STARTS_PLAYER_MESSAGE ". Inizia il %sgiocatore %d" FNRM " (" FORNG "%s" FNRM ")\n"
#define A_PLAYER_QUIT_SERVER_MESSAGE "\n\n" WARNING_CHAR "Il %sgiocatore %d" FNRM " (" FORNG "%s" FNRM ") ha abbandonato la partita "
#define DRAW_MESSAGE WARNING_CHAR "Pareggio.\n"
#define YOU_LOST_MESSAGE ERROR_CHAR "Hai perso!\n"
#define YOU_WON_MESSAGE SUCCESS_CHAR "Hai vinto!\n"
#define YOU_WON_FOR_QUIT_MESSAGE FGRN SUCCESS_CHAR "Hai vinto per abbandono dell'altro giocatore!\n"
#define OPPONENT_READY_MESSAGE "Avversario pronto!"
#define OPPONENT_TURN_MESSAGE " (Turno dell'avversario) "
#define YOUR_SYMBOL_IS_MESSAGE INFO_CHAR FORNG "%s" FNRM ", il tuo simbolo è " BOLD "%s%c" FNRM NO_BOLD "\n"
#define TIMEOUT_LOSS_MESSAGE ERROR_CHAR "Il tempo è scaduto. Hai perso per inattività\n"
#define AUTOPLAY_ENABLED_MESSAGE "\n" INFO_CHAR "Modalità AI attivata"
#define EASY_MODE_MESSAGE " (" FGRN "facile" FNRM ")"
#define MEDIUM_MODE_MESSAGE " (" FYEL "media" FNRM ")"
#define HARD_MODE_MESSAGE " (" FRED "difficile" FNRM ")"

// General success messages
#define SERVER_FOUND_SUCCESS FGRN SUCCESS_CHAR "Trovato TrisServer con PID = %d\n" FNRM

// Semaphore success messages
#define SEMAPHORE_OBTAINED_SUCCESS FGRN SUCCESS_CHAR "Semafori ottenuti\n" FNRM
#define SEMAPHORES_INITIALIZED_SUCCESS FGRN SUCCESS_CHAR "Semafori inizializzati\n" FNRM
#define SEMAPHORES_DISPOSED_SUCCESS FGRN SUCCESS_CHAR "Semafori deallocati\n" FNRM

// Shared memory success messages
#define MATRIX_INITIALIZED_MESSAGE FGRN SUCCESS_CHAR "Matrice inizializzata\n" FNRM
#define SHARED_MEMORY_OBTAINED_SUCCESS FGRN SUCCESS_CHAR "Memoria condivisa ottenuta (ID: %d)\n" FNRM
#define SHARED_MEMORY_ATTACHED_SUCCESS FGRN SUCCESS_CHAR "Memoria condivisa agganciata (@ %p)\n" FNRM
#define SHARED_MEMORY_DEALLOCATION_SUCCESS FGRN SUCCESS_CHAR "Memoria condivisa deallocata\n" FNRM

// Output messages
#define OUTPUT_RESTORED_SUCCESS FGRN SUCCESS_CHAR "Output ripristinato\n" FNRM

// ----------------- ERRORS ------------------

// General errors
#define USAGE_ERROR_SERVER "Uso: ./TrisServer <timeout> <playerOneSymbol> <playerTwoSymbol>"
#define USAGE_ERROR_CLIENT "Uso: ./TrisClient <username> [*]"
#define TOO_MANY_PLAYERS_ERROR "Troppi giocatori connessi. Riprova più tardi.\n"
#define SAME_USERNAME_ERROR "Il nome utente è già in uso. Riprova con un altro nome.\n"
#define INITIALIZATION_ERROR "Errore durante l'inizializzazione."
#define INVALID_MOVE_ERROR "Mossa non valida. Riprova: "
#define NO_SERVER_FOUND_ERROR "Nessun server trovato. Esegui TrisServer prima di eseguire TrisClient.\n"
#define SERVER_ALREADY_RUNNING_ERROR "Il server è già in esecuzione. Esegui un solo server alla volta.\n"
#define USERNAME_TOO_LONG_ERROR "Il nome utente non può superare i 30 caratteri."
#define USERNAME_TOO_SHORT_ERROR "Il nome utente deve contenere almeno 2 caratteri."
#define AUTOPLAY_NOT_ALLOWED_ERROR "Non è possibile giocare in modalità AI con un altro giocatore già collegato."

// Error codes
#define TOO_MANY_PLAYERS_ERROR_CODE -1
#define SAME_USERNAME_ERROR_CODE -2
#define AUTOPLAY_NOT_ALLOWED_ERROR_CODE -3

// Args errors
#define TIMEOUT_INVALID_CHAR_ERROR "Il valore specificato per il timeout non è valido."
#define TIMEOUT_TOO_LOW_ERROR "Il minimo timeout ammesso è di 5 secondi"
#define SYMBOLS_LENGTH_ERROR "I simboli dei giocatori devono essere di un solo carattere."
#define SYMBOLS_EQUAL_ERROR "I simboli dei giocatori devono essere diversi."

// Semaphore errors
#define SEMAPHORE_ALLOCATION_ERROR "Errore durante la creazione dei semafori."
#define SEMAPHORE_INITIALIZATION_ERROR "Errore durante l'inizializzazione dei semafori."
#define SEMAPHORE_DEALLOCATION_ERROR "Errore durante la deallocazione dei semafori."
#define SEMAPHORE_WAITING_ERROR "Errore durante l'attesa del semaforo."
#define SEMAPHORE_SIGNALING_ERROR "Errore durante la segnalazione del semaforo."
#define SEMAPHORE_GETTING_ERROR "Errore durante l'ottenimento di dati sul semaforo."

// Shared memory errors
#define SHARED_MEMORY_ALLOCATION_ERROR "Errore durante la creazione della memoria condivisa."
#define SHARED_MEMORY_ATTACH_ERROR "Errore durante l'aggancio della memoria condivisa."
#define SHARED_MEMORY_DEALLOCATION_ERROR "Errore durante la deallocazione della memoria condivisa."
#define SHARED_MEMORY_STATUS_ERROR "Errore durante l'ottenimento di dati sulla memoria condivisa."
#define SHARED_MEMORY_DETACH_ERROR "Errore durante l'ottenimento dell'indirizzo della memoria condivisa."

// Fork errors
#define FORK_ERROR "Errore durante la creazione di un processo figlio."

// Exec errors
#define EXEC_ERROR "Errore durante l'esecuzione di un programma."