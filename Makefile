CC = gcc
CFLAGS = -Wall -pedantic -g -fsanitize=address -lpthread
#CFLAGS = -Wall -pedantic -lpthread
SERVER_SRC = src/TrisServer.c
CLIENT_SRC = src/TrisClient.c
SERVER_BIN = bin/TrisServer
CLIENT_BIN = bin/TrisClient
AUX_FUNCTIONS = src/utils/data.h src/utils/globals.c src/utils/semaphores/semaphores.c src/utils/shared_memory/shared_memory.c

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC) $(AUX_FUNCTIONS)
	@echo "Compiling $@..."
	@$(CC) $(CFLAGS) -o $@ $^
	@echo "Done."

$(CLIENT_BIN): $(CLIENT_SRC) $(AUX_FUNCTIONS)
	@echo "Compiling $@..."
	@$(CC) $(CFLAGS) -o $@ $^
	@echo "Done."

.PHONY: clean

clean:
	@echo "Cleaning..."
	@rm -f $(SERVER_BIN) $(CLIENT_BIN)
	@echo "Done."