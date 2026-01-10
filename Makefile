CC = gcc
# Default compilation flags (can be overridden by environment)
CFLAGS ?= -Wall -Wextra -std=c11 -O2
DEBUGFLAGS = -g -O0

SERVER_SRC = src/server/server.c src/world/world.c src/simulation/simulation.c src/walker/walker.c
CLIENT_SRC = src/klient/klient.c

all: server client

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o server

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o client

clean:
	rm -f server client

debug: CFLAGS += $(DEBUGFLAGS)
debug: clean all

valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind.log ./server

.PHONY: all server client clean debug valgrind