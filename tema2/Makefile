build: server subscriber

utils.o: utils.c utils.h
	gcc -c utils.c

server: utils.o server.c
	gcc server.c utils.o -o server

subscriber: utils.o client.c
	gcc client.c utils.o -o subscriber

clean:
	rm server subscriber utils.o
