#ifndef _UTILS_H
#define _UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>

#define MAX_WAIT_CLIENTS 10
#define CONTENT_MAX_LEN 1500
#define TOPIC_MAX_LEN 50

// "stop connection", "sever shutdown", "still connected" toate 15 caractere + 1 term null
#define EXIT_MSG_LEN 16

// 3 puncte + 3 cifre * 4 numere + 1 terminator null = 16
#define IP_MAX_LEN 16
#define ID_MAX_LEN 11
#define INITIAL_CAPACITY 10

// subscribe + spatiu + lungime topic + spatiu + SF = 9 + 1 + 50 + 1 + 1 = 62
// unsubscribe + spatiu + lungime topic = 11 + 1 + 50 = 62
// 62 + newline-ul de la stdin + terminatorul null = 64
#define STDIN_CLIENT_LEN 64

/**
 * DIE - afiseaza mesajul de eroare si inchide programul
 * error - afiseaza mesajul de eroare si continua la urmatoare iteratie din bucla
 * error_func - afiseaza mesajul de eroare si iese din functie
 * error_ignore - afiseaza mesajul de eroare
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define error(assertion, call_description)	\
	if (assertion) {						\
		fprintf(stderr, "(%s, %d): ",		\
				__FILE__, __LINE__);		\
		perror(call_description);			\
		continue;							\
	}										\

#define error_func(assertion, call_description)	\
	if (assertion) {							\
		fprintf(stderr, "(%s, %d): ",			\
				__FILE__, __LINE__);			\
		perror(call_description);				\
		return;									\
	}											\

#define error_ignore(assertion, call_description)	\
	if (assertion) {								\
		fprintf(stderr, "(%s, %d): ",				\
				__FILE__, __LINE__);				\
		perror(call_description);					\
	}												\

void send_all(int sockfd, const char* buffer, uint32_t len);
char* recv_all(int sockfd, int* len);
int tcp_no_delay_opt(int sockfd);
int max(int a, int b);

/**
 * structura pachetului UDP primit de la clientii UDP de server
 * are topic-ul, tipul de date si content-ul ce trebuie trimis la clientii tcp
 */
typedef struct UDP_packet {
	char topic[TOPIC_MAX_LEN];
	uint8_t data_type;
	char content[CONTENT_MAX_LEN];
} UDP_Packet;

/**
 * structura pachetului trimis de clientii TCP la server
 * primeste index-ul comenzii(0 subscribe, 1 unsubscribe), daca este SF-ul
 * setat(pentru cazul cu subscribe) si topic-ul
 */
typedef struct TCP_packet_client {
	char topic[TOPIC_MAX_LEN];
	uint8_t command_index;
	uint8_t SF;
} TCP_Packet_Client;

/**
 * structura pachetului trimis de la server la clientii TCP
 * contine pachetul primit de la clientii UDP, port-ul si ip-ul
 * clientului UDP care a trimis pachetul
 */
typedef struct TCP_packet_server {
	uint16_t udp_client_port;
	char udp_client_ip[IP_MAX_LEN];
	UDP_Packet packet;
} TCP_Packet_Server;

#define UDP_PACKET_LEN sizeof(UDP_Packet)
#define TCP_PACKET_SERVER_LEN sizeof(TCP_Packet_Server)
#define TCP_PACKET_CLIENT_LEN sizeof(TCP_Packet_Client)

#endif