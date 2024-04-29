#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>

#include "utils/data.c"
#include "utils/utils.c"

bool init(int timeout, char playerOneSymbol, char playerTwoSymbol);

int main(int argc, char *argv[]){
    if(argc != N_ARGS){
        errExit(USAGE_ERROR);
    }

    int timeout = atoi(argv[1]);
    char playerOneSymbol = argv[2][0];
    char playerTwoSymbol = argv[3][0];

    if(!init(timeout, playerOneSymbol, playerTwoSymbol)){
        errExit(INIT_ERROR);
    }

    return EXIT_SUCCESS;
}

bool init(int timeout, char playerOneSymbol, char playerTwoSymbol){
    printWelcomeMessage();
    printLoadingMessage();
    return true;
}

