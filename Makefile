all: server client

server: server.c
	gcc -g -o server server.c -lpthread

client: client.c
	gcc -g -o client client.c -lpthread

clean:
	rm -f server client
