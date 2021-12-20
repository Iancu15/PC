CC=gcc
OPTS=-g -Wall

build: client

client: client.c utils.c parson.c helpers.c buffer.c
	$(CC) -o client client.c utils.c parson.c helpers.c buffer.c $(OPTS)

run: client
	./client

clean:
	rm -f *.o client
