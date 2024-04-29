CC = gcc
CFLAGS = -Wall -pedantic
SERVER_SRC = src/TrisServer.c
CLIENT_SRC = src/TrisClient.c
SERVER_OBJ = obj/TrisServer.o
CLIENT_OBJ = obj/TrisClient.o
SERVER_BIN = bin/TrisServer
CLIENT_BIN = bin/TrisClient

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJ)
	@echo "Linking $@..."
	@$(CC) $(CFLAGS) -o $@ $^
	@echo "Done."

$(CLIENT_BIN): $(CLIENT_OBJ)
	@echo "Linking $@..."
	@$(CC) $(CFLAGS) -o $@ $^
	@echo "Done."

$(SERVER_OBJ): $(SERVER_SRC)
	@echo "Compiling $@..."
	@$(CC) $(CFLAGS) -c -o $@ $^

$(CLIENT_OBJ): $(CLIENT_SRC)
	@echo "Compiling $@..."
	@$(CC) $(CFLAGS) -c -o $@ $^

.PHONY: clean

clean:
	@echo "Cleaning..."
	@rm -f $(SERVER_OBJ) $(SERVER_BIN) $(CLIENT_OBJ) $(CLIENT_BIN)
	@echo "Done."