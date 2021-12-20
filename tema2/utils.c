#include "utils.h"

/**
 * Trimite len octeti din buffer la destinatia socket-ului sockfd
 */
void send_all(int sockfd, const char* buffer, uint32_t len) {
    uint32_t bytes_remaining = len;
    uint32_t bytes_sent = 0;
    int wc;

    // trimite in network byte order lungimea pachetului trimis
    while (bytes_sent < 4) {
        uint32_t network_len = htonl(len);
        wc = send(sockfd, &network_len, sizeof(uint32_t), 0);
        DIE(wc <= 0, "send error");
        bytes_sent += wc;
    }

    // cat timp mai sunt octeti de trimis, sunt trimisi
    // se va trimite incepand de la pozitia buffer[bytes_sent] pentru
    // ca octetii de la 0 la bytes_sent - 1 au fost deja trimisi
    bytes_sent = 0;
    while(bytes_remaining > 0) {
        wc = send(sockfd, buffer + bytes_sent, bytes_remaining, 0);
        DIE(wc <= 0, "send error");
        bytes_remaining -= wc;
        bytes_sent += wc;
    }
}

/**
 * Intoarce un buffer cu pachetul primit de la socket-ul sockfd, se da la intrare
 * o lungime maxima len a numarul de octeti ce ar trebui primit, aceasta lungime
 * fiind actualizata cu cati au fost primiti de fapt
 */
char* recv_all(int sockfd, int* len) {
    uint32_t bytes_recieved = 0;
    int wc;
    char* buffer = (char*) malloc(sizeof(char) * (*len + sizeof(uint32_t) + 1));
    DIE(!buffer, "malloc");

    // fac rost de lungimea sirului
    while (bytes_recieved < 4) {
        wc = recv(sockfd, buffer + bytes_recieved, sizeof(uint32_t) - bytes_recieved, 0);
        DIE(wc < 0, "recv error");
        bytes_recieved += wc;
    }

    // am primit-o in network byte order, asa ca o convertesc
    uint32_t size_of_packet;
    memcpy(&size_of_packet, buffer, sizeof(uint32_t));
    size_of_packet = ntohl(size_of_packet);

    // cat timp mai sunt octeti de primit, ii primesc
    // cum bytes recieved e deja 4 de la lungime, atunci scad 4 pentru ca size_of_packet e lungimea
    // sirului fara cei 4 octeti de lungime, la fel scad -(bytes_recieved - sizeof(uint32_t)) la
    // recv
    while(bytes_recieved - sizeof(uint32_t) < size_of_packet) {
        wc = recv(sockfd, buffer + bytes_recieved, size_of_packet - bytes_recieved + sizeof(uint32_t), 0);
        DIE(wc < 0, "recv error");
        bytes_recieved += wc;
    }

    DIE(bytes_recieved <= 0, "no byte received");

    // se actualizeaza lungimea maxima cu lungimea efectiva pentru apelant
    // se intoarce buffer-ul incepand de la pozitia 4 pentru a ignora lungimea
    *len = bytes_recieved - sizeof(uint32_t);
    return buffer + 4;
}

/**
 * Dezactiveaza algoritmul lui Neagle pentru socket-ul sockfd primit la intrare
 */
int tcp_no_delay_opt(int sockfd) {
    int enable = 1;
	int setsockopt_status = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
    if (setsockopt_status < 0) {
        return -1;
    }

    return 1;
}

/**
 * Face maxim-ul dintre a si b
 */
int max(int a, int b) {
    if (a > b) {
        return a;
    }

    return b;
}