/*
*  	Protocoale de comunicatii: 
*  	Laborator 6: UDP
*	mini-server de backup fisiere
*/

#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>

#include "helpers.h"

void usage(char*file)
{
	fprintf(stderr,"Usage: %s server_port file\n",file);
	exit(0);
}

/*
*	Utilizare: ./server server_port nume_fisier
*/
int main(int argc,char**argv)
{
	int fd;
	

	if (argc != 3)
		usage(argv[0]);
	
	struct sockaddr_in my_sockaddr;
	char buf[BUFLEN];


	/*Deschidere socket*/
	int socketfd = socket(AF_INET, SOCK_DGRAM,0);
	
	/*Setare struct sockaddr_in pentru a asculta pe portul respectiv */
	my_sockaddr.sin_family = AF_INET;
	my_sockaddr.sin_port = htons(atoi(argv[1]));
	my_sockaddr.sin_addr.s_addr = INADDR_ANY;
	
	/* Legare proprietati de socket */
	bind(socketfd, (struct sockaddr*) &my_sockaddr, sizeof(my_sockaddr));
	
	
	/* Deschidere fisier pentru scriere */
	DIE((fd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1, "open file");
	
	/*
	*  cat_timp  mai_pot_citi
	*		citeste din socket
	*		pune in fisier
	*/
	socklen_t len = sizeof(my_sockaddr); 
	while(1) {
		int rd = recvfrom(socketfd, buf, sizeof(buf), 0, (struct sockaddr*) &my_sockaddr, &len);
		if (rd <= 0) {
			break;
		}

		write(fd, buf, rd);
	}

	/*Inchidere socket*/	
	close(socketfd);
	
	/*Inchidere fisier*/
	close(fd);

	return 0;
}
