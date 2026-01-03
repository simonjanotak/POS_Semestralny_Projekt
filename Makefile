CC = gcc
CFLAGS = -Wall -Wextra -std=c11

SERVER_SRC = src/server/server.c
CLIENT_SRC = src/klient/klient.c

all: server client

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) $(SERVER_SRC) -o server

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) $(CLIENT_SRC) -o client

clean:
	rm -f server client
