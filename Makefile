CC = gcc
CFLAGS = -g -Wall -Wextra -pthread
TARGETS = server client

all: $(TARGETS)

server: server.c
	$(CC) $(CFLAGS) server.c -o server

client: client.c
	$(CC) $(CFLAGS) client.c -o client

clean:
	rm -f $(TARGETS)
