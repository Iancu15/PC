#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}

void send_all(int socket, const char* buff, size_t len) {
    size_t bytes_remaining = len;
    size_t bytes_sent = 0;
    while(bytes_remaining > 0) {
        int wc = send(socket, &buff[bytes_sent], bytes_remaining, 0);
        DIE(wc <= 0, "send");
        bytes_remaining -= wc;
        bytes_sent += wc;
    }
}

int main(int argc, char *argv[])
{
	int sockfd, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	if (argc < 3) {
		usage(argv[0]);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2]));
	ret = inet_aton(argv[1], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	fd_set read_fds;
	FD_ZERO(&read_fds);
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);
	
	int fd_max = STDIN_FILENO > sockfd ? STDIN_FILENO: sockfd;

	while(1) {
		fd_set tmp_fds = read_fds;
		select(fd_max+1, &tmp_fds, NULL, NULL, NULL);

		if(FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			// citim de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);
			if (strncmp(buffer, "exit", 4) == 0) {
				break;
			}

			// trimitem catre server
			send_all(sockfd, buffer, BUFLEN);
		}

		if(FD_ISSET(sockfd, &tmp_fds)) {
			// citim de pe socket si afisam pe ecran
			int read = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
	        if(read > 0) {
	            buffer[read] = '\0';
                if (!strncmp(buffer, "exit", 4)) {
                    exit(-1);
                }
                
                if (buffer[0] == 1) {
                    printf("S-a adaugat cu succes!\n");
                } else if (buffer[0] == -1) {
                    fprintf(stderr, "Jocul nu a inceput!\n");
                } else if (buffer[0] == -2) {
                    fprintf(stderr, "E runda oponentului!\n");
                } else if (buffer[0] == -3) {
                    fprintf(stderr, "Casuta e deja completata!\n");
                } else if (buffer[0] == 2) {
                    printf("Ai castigat!\n");
                } else if (buffer[0] == 3) {
                    printf("Ai pierdut!\n");
                } else {
                    printf("UNDEFINED\n");
                }
	        }
	        else if(read == 0){
	   			fprintf(stderr, "S-a terminat jocul\n");
	   			exit(-1);
            }
	        else {
	            DIE(1, "read");
	        }

	        memset(buffer, 0, BUFLEN);
		}
	}

	close(sockfd);

	return 0;
}
