all: server client

server: server.c
	gcc -g -o server server.c -lpthread -lrt

client: client.c
	gcc -g -o client client.c -lpthread -lrt

clean:
	rm -f server client
