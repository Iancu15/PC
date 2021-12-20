/*
*  	Protocoale de comunicatii: 
*  	Laborator 6: UDP
*	client mini-server de backup fisiere
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
	fprintf(stderr,"Usage: %s ip_server port_server file\n",file);
	exit(0);
}

/*
*	Utilizare: ./client ip_server port_server nume_fisier_trimis
*/
int main(int argc,char**argv)
{
	if (argc != 4)
		usage(argv[0]);
	
	int fd;
	struct sockaddr_in to_station;
	char buf[BUFLEN];

	memset(&to_station, 0, sizeof(to_station));


	/*Deschidere socket*/
	int socketfd = socket(AF_INET, SOCK_DGRAM,0);
	
	/* Deschidere fisier pentru citire */
	DIE((fd=open(argv[3], O_RDONLY)) == -1, "open file");
	
	/*Setare struct sockaddr_in pentru a specifica unde trimit datele*/
	to_station.sin_family = AF_INET;
	to_station.sin_port = htons(atoi(argv[2]));
	inet_aton(argv[1], &to_station.sin_addr);
	
	/*
	*  cat_timp  mai_pot_citi
	*		citeste din fisier
	*		trimite pe socket
	*/	
	socklen_t len = sizeof(to_station); 
	while(1){
		int rd = read(fd, buf, sizeof(buf)) ;
		sendto(socketfd, buf, rd, 0, (struct sockaddr*) &to_station, len);
		if (rd <= 0 ){
			sendto(socketfd, buf, 0, 0, (struct sockaddr*) &to_station, len);
			break;
		}
	}

	/*Inchidere socket*/
	close(socketfd);
	
	/*Inchidere fisier*/
	close(fd);

	return 0;
}
