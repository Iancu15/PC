#include "utils.h"

/**
 * Numarul primit la intrare se imparte la 10^neg_pow si se
 * afiseaza cu neg_pow zecimale
 */
void process_float(uint32_t number, uint8_t neg_pow) {
    float float_number = number;
    for (int i = neg_pow; i > 0; i--) {
        float_number /= 10;
    }

    // formatul pentru afisarea unui numar cu n zecimale este
    // "%.nf", asa ca in loc de n o sa pun neg_pow
    char start_print[3] = "%.";
    char precision[10];
    sprintf(precision, "%hhd", neg_pow);
    char end_print[3] = "f\n";
    char* print_opts = strcat(start_print, strcat(precision, end_print));
    printf(print_opts, float_number);
}

/**
 * Afiseaza mesajul aferent pachetului primit de la server
 */
void print_msg(TCP_Packet_Server* recv_packet) {
    printf("%s:%d", recv_packet->udp_client_ip, ntohs(recv_packet->udp_client_port));
    printf(" - %s - ", recv_packet->packet.topic);
    // data_type e de un octet asa ca nu trebuie sa fac nicio conversie pentru ca
    // un octet e la fel in oricare endiannes
    uint8_t data_type = recv_packet->packet.data_type;
    if (data_type == 0) {
        printf("INT - ");
        // iau primul octet din content si daca acesta este 1 atunci afisez semnul minus
        uint8_t sign = recv_packet->packet.content[0];
        error_func(sign != (uint8_t) 0 && sign != (uint8_t) 1, "Undefined sign bit");
        if (sign == 1) {
            printf("-");
        }

        // octetii de la 1 la 4 sunt numarul ce va trebui afisat, fiind 4 octeti trebuie
        // sa schimb endiannesul de la network la host
        uint32_t data;
        memcpy(&data, &(recv_packet->packet.content[1]), sizeof(uint32_t));
        printf("%u\n", ntohl(data));
        return;
    }

    if (data_type == 1) {
        printf("SHORT_REAL - ");
        uint16_t data;
        memcpy(&data, recv_packet->packet.content, sizeof(uint16_t));

        // impart numarul la 100 si il afisez cu 2 zecimale
        float result = (float) ntohs(data) / 100;
        printf("%.2f\n", result);
        return;
    }

    if (data_type == 2) {
        printf("FLOAT - ");
        uint8_t sign = recv_packet->packet.content[0];
        error_func(sign != (uint8_t) 0 && sign != (uint8_t) 1, "Undefined sign bit");
        if (sign == 1) {
            printf("-");
        }

        // data e la fel ca la INT, diferenta e ca pe pozitia 5 avem exponentul
        // corespondent numarului de zecimale, dau numarul dupa conversie alaturi
        // de exponent functiei process_float explicata mai sus
        uint32_t data;
        memcpy(&data, &(recv_packet->packet.content[1]), sizeof(uint32_t));
        uint8_t neg_pow = recv_packet->packet.content[5];
        error_func(neg_pow < (uint8_t) 0, "Undefined float power");
        process_float(ntohl(data), neg_pow);
        return;
    }

    // daca e string pur si simplu il afisez
    if (data_type == 3) {
        printf("STRING - %s\n", recv_packet->packet.content);
        return;
    }

    error_func(1, "Undefined data type");
}

/**
 * Verifica daca sirul de caractere dat la intrare este un format valid de ip
 */
void check_ip(char* ip) {
    // tokenizez dupa . pentru a face rost de numerele dintre puncte
    char* token = strtok(ip, ".");
    int number_of_tokens = 1;
    int octet = atoi(token);

    /**
    * atoi da 0 pentru cazul in care parametrul dat nu este un numar de tip int, insa
    * ip-urile pot avea numere 0, asa ca input-ul este invalid doar daca pe langa faptul
    * ca atoi a dat 0 si primul caracter e diferit de '0', insa de asemenea '015' spre ex.
    * nu este un numar valid pentru formatul ipv4 asa ca acesta trebuie sa fie de lungime
    * fix 1 sa fie valid si anume fix sirul "0"
    */
    DIE(octet == 0 && token[0] != '0' && strlen(token) != 1, "ip shouldn't have characters");
    DIE(octet < 0 || octet > 255, "invalid ip decimal octets");
    while (1) {
        token = strtok(NULL, ".");
        if (token == NULL) {
            break;
        }

        number_of_tokens++;
        octet = atoi(token);
        DIE(octet == 0 && token[0] != '0' && strlen(token) != 1, "ip shouldn't have characters");
        DIE(octet < 0 || octet > 255, "invalid ip decimal octets");
    }

    // ipv4 e un format ce are doar 4 numere separate de puncte, daca sunt mai multe sau mai putine
    // inputul este declarat invalid
    DIE(number_of_tokens != 4, "too many/few dots in ip expression");
}

int main(int argc, char* argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(argc != 4, "invalid number of arguments");
    char* id = argv[1];
    DIE(strlen(id) > 10, "ID is too big");
    char* ip = argv[2];
    uint16_t port = atoi(argv[3]);
    DIE(!port, "invalid port argument");
    DIE(strlen(ip) > IP_MAX_LEN, "invalid ip length");

    // am duplicat sirul pentru ca strtok distruge sirul initial
    check_ip(strdup(argv[2]));
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket error");
    tcp_no_delay_opt(sockfd);

    /**
     * socket-ul creat pentru a comunica cu serverul este conectat la adresa si portul server-ului
     * este trimis apoi id-ul clientului catre server pentru ca acesta sa stie cu cine comunica
     * mai exact
     */
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	DIE(inet_aton(argv[2], &server_addr.sin_addr) == 0, "inet_aton error");
    DIE(connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0, "connect error");
    send_all(sockfd, id, ID_MAX_LEN);

    /**
     * Se creeaza setul de citire si se adauga socketul de comunicare cu serverul alaturi de
     * file descriptorul pentru citirea de la tastatura
     */
    fd_set* read_set = (fd_set*) malloc(sizeof(fd_set));
    DIE(!read_set, "set allocation");
    FD_ZERO(read_set);
    FD_SET(STDIN_FILENO, read_set);
    FD_SET(sockfd, read_set);
    int fdmax = max(sockfd, STDIN_FILENO);
    while (1) {
        fd_set* tmp_set = (fd_set*) malloc(sizeof(fd_set));
        DIE(!tmp_set, "temp set allocation");
        memcpy(tmp_set, read_set, sizeof(fd_set));
        DIE(select(fdmax + 1, tmp_set, NULL, NULL, NULL) < 0, "select error");

        /* caz comanda de la tastatura */
        if (FD_ISSET(STDIN_FILENO, tmp_set)) {
            char buffer[STDIN_CLIENT_LEN];
            memset(buffer, 0, STDIN_CLIENT_LEN);
            fgets(buffer, STDIN_CLIENT_LEN, stdin);

            /**
             * Daca comanda este "exit", atunci se trimite un mesaj la server
             * sa opreasca conexiunea si apoi se inchide programul
             */
            char* token = strtok(buffer, " \n");
            if (strcmp(token, "exit") == 0) {
                char buffer[EXIT_MSG_LEN] = "stop connection";
                send_all(sockfd, buffer, EXIT_MSG_LEN);
                DIE(close(sockfd) < 0, "close connection");
                exit(1);
            }

            /**
             * daca cumva comanda este subscribe atunci o consideram comanda cu
             * indexul 0, daca este unsubscribe apoi are indexul 1
             */
            uint8_t which_command = -1;
            if (strcmp(token, "subscribe") == 0) {
                which_command = 0;
            } else if (strcmp(token, "unsubscribe") == 0) {
                which_command = 1;
            }

            // daca nu este nici "subscribe" nici "unsubscribe" atunci se da
            // mesaj de eroare pentru ca nu mai sunt alte comenzi
            error(which_command == (uint8_t) -1, "invalid command");

            // salvez topic-ul in sirul topic
            token = strtok(NULL, " \n");
            error(!token, "no topic chosen");
            int topic_len = strlen(token);
            error(topic_len > TOPIC_MAX_LEN, "topic length too big");
            char* topic = (char*) malloc(sizeof(char) * topic_len);
            DIE(!topic, "topic allocation");
            memcpy(topic, token, topic_len);
            topic[strlen(token)] = '\0';

            uint8_t SF = 0;
            if (which_command == 0) {
                token = strtok(NULL, " \n");
                error(!token, "no SF option chosen");
                int option = atoi(token);

                // aceeasi idee de verificare a corectitudinii ca la ip_check
                DIE((option != 0 && token[0] != '0' && strlen(token) != 1) && option != 1, "invalid SF option");
                SF = option;
            }

            /**
             * Creez pachetul de trimite in care adaug indexul comenzii, valoarea de adevar a SF
             * (se va trimite 0 pentru unsubscribe, nu conteaza totusi ca nu va fi accesat campul
             * in acel caz de server) si topicul
             * Il trimit catre server dupa ce copiez structura intr-un sir de caractere pentru ca
             * functiile mele de recv si send functioneaza doar pe char*
             */
            TCP_Packet_Client* send_pkt = (TCP_Packet_Client*) malloc(sizeof(TCP_Packet_Client));
            memcpy(&(send_pkt->topic), topic, TOPIC_MAX_LEN);
            send_pkt->command_index = which_command;
            send_pkt->SF = SF;
            char* packet_buffer = (char*) malloc(TCP_PACKET_CLIENT_LEN);
            DIE(!packet_buffer, "malloc");
            memcpy(packet_buffer, send_pkt, TCP_PACKET_CLIENT_LEN);
            send_all(sockfd, packet_buffer, TCP_PACKET_CLIENT_LEN);
            if (which_command == 0) {
                printf("Subscribed to topic.\n");
            } else {
                printf("Unsubscribed from topic.\n");
            }

            free(packet_buffer);
            free(send_pkt);
        }

        /* caz primire pachet de la server */
        if (FD_ISSET(sockfd, tmp_set)) {
            int buffer_len = TCP_PACKET_SERVER_LEN;
			char* buffer = recv_all(sockfd, &buffer_len);
            DIE(!buffer, "buffer is NULL");
            error(buffer_len > TCP_PACKET_SERVER_LEN, "Undefined packet length");
            if (!strcmp(buffer, "server shutdown") || !strcmp(buffer, "still connected")) {
                DIE(close(sockfd) < 0, "close connection");
                exit(1);
            }

            // copiez buffer-ul in structura aferenta si apelez functia print_msg pentru a
            // afisa continutul
            TCP_Packet_Server* recv_packet = (TCP_Packet_Server*) malloc(TCP_PACKET_SERVER_LEN);
            DIE(!recv_packet, "malloc");
            memcpy(recv_packet, buffer, TCP_PACKET_SERVER_LEN);
            print_msg(recv_packet);
            free(recv_packet);
		}

        free(tmp_set);
    }

    close(sockfd);
}