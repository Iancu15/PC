#include "utils.h"

/*** CODUL PENTRU REGISTRUL DE TOPIC-URI ***/

typedef struct tcp_client {
    int sockfd;
    char id[ID_MAX_LEN];
    uint8_t is_connected;
} TCP_Client;

typedef struct register_client {
    TCP_Client* client;
    uint8_t SF;
} Register_Client;

typedef struct register_entry {
    char topic[TOPIC_MAX_LEN];
    Register_Client* clients;
    unsigned int number_of_clients;
    unsigned int capacity;
} Register_Entry;

/**
 * Registrul contine o lista de entry-uri pentru fiecare topic in parte
 * Entry-ul contine o lista cu clientii abonati la topic, fiecare intrare de client din
 * lista contine informatiile de interes: daca client-ul s-a abonat cu SF activat,
 * informatiile aferente clientului(socket-ul pe care se comunica cu el, id-ul sau
 * si starea de conectivitate)
 */
typedef struct topic_register {
    Register_Entry** topic_list;
    unsigned int size;
    unsigned int capacity;
} Topic_Register;

Topic_Register* create_topic_register(unsigned int initial_capacity) {
    Topic_Register* t_register = (Topic_Register*) malloc(sizeof(Topic_Register));
    DIE(!t_register, "topic register malloc error");
    t_register->topic_list = (Register_Entry**) malloc(sizeof(Register_Entry*) * initial_capacity);
    DIE(!t_register->topic_list, "topic list malloc error");
    t_register->size = 0;
    t_register->capacity = initial_capacity;

    return t_register;
}

void free_topic_register(Topic_Register* t_register) {
    for (int i = 0; i < t_register->size; i++) {
        free(t_register->topic_list[i]);
    }

    free(t_register->topic_list);
    free(t_register);
}

/**
 * Cauta intrarea aferenta unui topic
 */
Register_Entry* search_topic_entry(Topic_Register* t_register, const char* topic) {
    for (int entry_index = 0; entry_index < t_register->size; entry_index++) {
        Register_Entry* entry = t_register->topic_list[entry_index];
        if (strncmp(entry->topic, topic, strlen(topic)) == 0) {
            return entry;
        }
    }

    return NULL;
}

/**
 * Cauta un client in intrarea unui topic, daca acesta se regaseste(clientul este abonat la topic),
 * atunci se intoarce index-ul acestuia din vectorul de clienti, altfel se intoarce -1
 */
int topic_list_client_index(Register_Entry* entry, TCP_Client* client) {
    for (int client_index = 0; client_index < entry->number_of_clients; client_index++) {
        if (!strcmp(entry->clients[client_index].client->id, client->id)) {
            return client_index;
        }
    }

    return -1;
}

/**
 * Aboneaza un client la un topic
 */
void subscribe_client_to_topic(Topic_Register* t_register, const char* topic, 
                                                        TCP_Client* client, uint8_t SF) {
    Register_Entry* searched_entry = search_topic_entry(t_register, topic);
    // daca topic-ul nu are o intrare, se creeaza una si se adauga clientul la aceasta
    if (searched_entry == NULL) {
        int last_index = t_register->size++;
        t_register->topic_list[last_index] = (Register_Entry*) malloc(sizeof(Register_Entry));
        Register_Entry* new_entry = t_register->topic_list[last_index];
        DIE(!new_entry, "register entry malloc error");
        memcpy(new_entry->topic, topic, TOPIC_MAX_LEN);
        new_entry->clients = (Register_Client*) malloc(MAX_WAIT_CLIENTS * sizeof(Register_Client));
        DIE(!new_entry->clients, "client list malloc new entry error");
        new_entry->clients[0].client = client;
        new_entry->clients[0].SF = SF;
        new_entry->number_of_clients = 1;
        new_entry->capacity = MAX_WAIT_CLIENTS;
        if (t_register->size == t_register->capacity) {
            t_register->capacity *= 2;
            size_t new_size = t_register->capacity * sizeof(Register_Entry*);
            t_register->topic_list = realloc(t_register->topic_list, new_size);
            DIE(!t_register->topic_list, "realloc topic_list error");
        }

        return;
    }

    // daca topic-ul are o intrare si clientul nu este deja abonat la topic, se pune
    // pe ultima pozitie din vectorul de clienti alaturi de optiunea SF
    int client_index = topic_list_client_index(searched_entry, client);
    error_func(client_index != -1, "client is already subscribed to topic");
    searched_entry->clients[searched_entry->number_of_clients++].client = client;
    searched_entry->clients[searched_entry->number_of_clients - 1].SF = SF;
    if (searched_entry->capacity == searched_entry->number_of_clients) {
        searched_entry->capacity *= 2;
        size_t new_size = searched_entry->capacity * sizeof(Register_Client);
        searched_entry->clients = realloc(searched_entry->clients, new_size);
        DIE(!searched_entry->clients, "realloc searched_entry->clients");
    }
}

/**
 * Dezaboneaza un client de la un topic
 */
void unsubscribe_client_from_topic(Topic_Register* t_register, const char* topic,
                                                                            TCP_Client* client) {
    Register_Entry* searched_entry = search_topic_entry(t_register, topic);
    error_func(searched_entry == NULL, "cannot unsubscribe from inexistent topic");

    int client_index = topic_list_client_index(searched_entry, client);
    error_func(client_index == -1, "client is already unsubscribed from topic");

    // daca client-ul nu se afla pe ultima pozitie din vector, atunci se copiaza
    // pe pozitia acestuia ultimul client din vector
    int last_index = searched_entry->number_of_clients - 1;
    if (client_index != last_index) {
        searched_entry->clients[client_index] = searched_entry->clients[last_index];
    }

    // se scade dimensiunea ca ultimul client sa fie out of range
    // ultimul client fiind fie clientul care s-a dezabonat fie clientul care deja l-a
    // inlocuit pe acesta
    searched_entry->number_of_clients--;
}

/*** CODUL PENTRU LISTA DE PACHETE SF ***/

/**
 * Retine pachetul ce trebuie trimis la clienti si numarul de clienti catre
 * care mai trebuie sa trimita
 * in momentul in care se ajunge reimaining_number_of_clients la 0 este de
 * datoria mea sa il elimin din lista de pachete
 */
typedef struct SF_packet {
    TCP_Packet_Server tcp_packet;
    uint32_t remaining_number_of_clients;
} SF_Packet;

/**
 * Lista de pachete restante
 */
typedef struct SF_packets {
    SF_Packet* packets;
    uint32_t size;
    uint32_t capacity;
} SF_Packets_List;

SF_Packets_List* create_sf_packets_list(unsigned int capacity) {
    SF_Packets_List* sf_pckt_list = (SF_Packets_List*) malloc(sizeof(SF_Packets_List));
    DIE(!sf_pckt_list, "malloc");
    sf_pckt_list->capacity = capacity;
    sf_pckt_list->size = 0;
    sf_pckt_list->packets = (SF_Packet*) malloc(sizeof(SF_Packet) * capacity);
    DIE(!sf_pckt_list->packets, "malloc");

    return sf_pckt_list;
}

/**
 * Adauga un pachet primit ca parametru in lista de pachete restante
 */
void add_sf_packet(SF_Packets_List* sf_pckt_list, TCP_Packet_Server pkt,
                                                                Topic_Register* t_register) {
    memcpy(&(sf_pckt_list->packets[sf_pckt_list->size++].tcp_packet), &pkt, TCP_PACKET_SERVER_LEN);
    // numarul de clienti catre care trebuie sa trimita sunt numarul de clienti abonati la topicul
    // aferent pachetului
    Register_Entry* entry = search_topic_entry(t_register, pkt.packet.topic);
    unsigned int number_of_clients = entry->number_of_clients;
    sf_pckt_list->packets[sf_pckt_list->size - 1].remaining_number_of_clients = number_of_clients;

    if (sf_pckt_list->size == sf_pckt_list->capacity) {
        sf_pckt_list->capacity *= 2;
        size_t new_size = sizeof(SF_Packet) * sf_pckt_list->capacity;
        sf_pckt_list->packets = realloc(sf_pckt_list->packets, new_size);
        DIE(!sf_pckt_list->packets, "realloc");
    }
}

/**
 * Trimite pachetul primit ca parametru catre toti clientii abonati la topicul acestuia
 */
void send_topic_to_subscribers(Topic_Register* t_register, TCP_Packet_Server pckt,
                                                                SF_Packets_List* sf_pckt_list) {
    // se cauta intrarea topic-ului continut in pachet, daca o astfel de intrare nu exista
    // atunci topic-ul nu are niciun client abonat
    Register_Entry* searched_entry = search_topic_entry(t_register, pckt.packet.topic);
    if (searched_entry == NULL) {
        return;
    }

    // se adauga pachetul in lista de pachete
    add_sf_packet(sf_pckt_list, pckt, t_register);
    char* buffer = (char*) malloc(TCP_PACKET_SERVER_LEN);
    DIE(!buffer, "malloc");
    memcpy(buffer, &pckt, TCP_PACKET_SERVER_LEN);

    /**
     * Pentru fiecare client la care este abonat daca:
     * -este conectat, atunci ii trimite pachetele
     * -este deconectat, dar are SF dezactivat, atunci doar se considera
     * ca pachetul si-a facut treaba si isi decrementeaza numarul de client(pentru ca SF
     * este 0, daca este deconectat nu va mai primi vreodata pachetul)
     * -daca este deconectat si SF activat atunci nu se face nimic pentru ca urmeaza
     * ca atunci cand se conecteaza sa isi primeasca pachetul(inca este luat astfel
     * in considerare in remaining_number_of_clients)
     */
    for (int client_index = 0; client_index < searched_entry->number_of_clients; client_index++) {
        Register_Client r_client = searched_entry->clients[client_index];
        if (r_client.client->is_connected) {
            send_all(r_client.client->sockfd, buffer, TCP_PACKET_SERVER_LEN);
            sf_pckt_list->packets[sf_pckt_list->size - 1].remaining_number_of_clients--;
        }

        if (r_client.client->is_connected == 0 && r_client.SF == 0) {
            sf_pckt_list->packets[sf_pckt_list->size - 1].remaining_number_of_clients--;
        }
    }

    // daca pachetul a trimis la toti clientii care trebuia si nu mai are nimic restant
    // atunci este scos din vectorul de pachete(cum a fost adaugat ultimul, se afla
    // pe ultima pozitie, decrementand size e scos din range)
    if (sf_pckt_list->packets[sf_pckt_list->size - 1].remaining_number_of_clients == 0) {
        sf_pckt_list->size--;
    }

    free(buffer);
}

/**
 * Trimite catre client-ul primit ca parametru toate pachetele restante fata de acesta din lista
 * de pachete
 */
void send_remnant_SF_packets(Topic_Register* t_register, SF_Packets_List* sf_pckt_list,
                                                                            TCP_Client* client) {
    for (int packet_index = 0; packet_index < sf_pckt_list->size; packet_index++) {
        // se iese cu DIE pentru ca ar fi o eroare de implementare pentru ca m-am asigurat
        // sa fie scos din vector daca nu mai are clienti restanti
        SF_Packet pckt = sf_pckt_list->packets[packet_index];
        DIE(pckt.remaining_number_of_clients == 0, "clients already exhausted");
        Register_Entry* entry = search_topic_entry(t_register, pckt.tcp_packet.packet.topic);

        // un pachet cu un topic fara clienti abonati n-ar trebui sa fie in lista de pachete
        // pentru ca un client isi primeste pachetele restante la pornire si astfel nu se poate
        // dezabona inainte sa le primeasca, ar fi o eroare de implementare
        DIE(!entry, "couldn't find entry for sf packet");

        // daca clientul nu e abonat la topicul pachetului, pachetul este ignorat
        int client_index = topic_list_client_index(entry, client);
        if (client_index == -1) {
            continue;
        }

        // daca totusi este abonat la topic-ul pachetului, dar cu SF dezactivat
        // atunci este ignorat pentru ca motivul pentru care inca este in lista de
        // pachete este sa trimita catre clientii care s-au abonat la el cu SF activat
        if (entry->clients[client_index].SF == 0) {
            continue;
        }

        // daca se ajunge aici atunci clientul este abonat la topic-ul pachetului cu SF
        // activat, se trimite pachetul si se considera deeds done
        char* buffer = (char*) malloc(TCP_PACKET_SERVER_LEN);
        DIE(!buffer, "malloc");
        memcpy(buffer, &(pckt.tcp_packet), TCP_PACKET_SERVER_LEN);
        send_all(client->sockfd, buffer, TCP_PACKET_SERVER_LEN);
        free(buffer);
        sf_pckt_list->packets[packet_index].remaining_number_of_clients--;
        if (sf_pckt_list->packets[packet_index].remaining_number_of_clients == 0) {
            int last_index = sf_pckt_list->size - 1;
            if (packet_index != last_index) {
                sf_pckt_list->packets[packet_index] = sf_pckt_list->packets[last_index];
                // pentru a testa din nou packet_index pentru ca acum are vechiul last_index
                packet_index--;
            }

            sf_pckt_list->size--;
        }
    }
} 

/*** CODUL PENTRU LISTA CU INFORMATII CLIENTI ***/

/**
 * Retine o lista cu clientii TCP care s-au conectat vreodata la server
 */
typedef struct clientinfo {
    TCP_Client** clients;
    unsigned int size;
    unsigned int capacity;
} Client_Info_List;

Client_Info_List* create_client_info_list(unsigned int initial_capacity) {
    Client_Info_List* c_info_list = (Client_Info_List*) malloc(sizeof(Client_Info_List));
    DIE(!c_info_list, "malloc");
    c_info_list->clients = (TCP_Client**) malloc(sizeof(TCP_Client*) * initial_capacity);
    DIE(!c_info_list->clients, "malloc");
    c_info_list->capacity = initial_capacity;
    c_info_list->size = 0;

    return c_info_list;
}

void free_client_info_list(Client_Info_List* c_info_list) {
    for (int i = 0; i < c_info_list->size; i++) {
        free(c_info_list->clients[i]);
    }

    free(c_info_list->clients);
    free(c_info_list);
}

/**
 * Cauta client-ul dupa ID si intoarce toate informatiile despre client
 */
TCP_Client* find_client_by_id(Client_Info_List* c_info_list, char* id) {
    for (int i = 0; i < c_info_list->size; i++) {
        if (!strcmp(c_info_list->clients[i]->id, id)) {
            return c_info_list->clients[i];
        }
    }

    return NULL;
}

/**
 * Adauga un client in lista de clienti
 */
void add_client_to_info_list(Client_Info_List* c_info_list, int sockfd, char* id) {
    TCP_Client* client = find_client_by_id(c_info_list, id);
    DIE(client != NULL, "client already connected");
    c_info_list->clients[c_info_list->size++] = (TCP_Client*) malloc(sizeof(TCP_Client));
    client = c_info_list->clients[c_info_list->size - 1];
    DIE(!client, "malloc");
    client->sockfd = sockfd;
    memcpy(&(client->id), id, ID_MAX_LEN);

    client->is_connected = 1;
    client->sockfd = sockfd;
    if (c_info_list->size == c_info_list->capacity) {
        c_info_list->capacity *= 2;
        size_t new_size = sizeof(TCP_Client) * c_info_list->capacity;
        c_info_list->clients = realloc(c_info_list->clients, new_size);
        DIE(!c_info_list->clients, "c_info_list realloc error");
    }
}

/**
 * Cauta un client dupa socket-ul pe care sever-ul comunica cu el
 */
TCP_Client* find_client_by_sock(Client_Info_List* c_info_list, int sockfd) {
    for (int i = 0; i < c_info_list->size; i++) {
        if (c_info_list->clients[i]->sockfd == sockfd) {
            return c_info_list->clients[i];
        }
    }

    return NULL;
}

/*** FUNCTII AUXILIARE ***/

/**
 * Primeste socket-ul udp si pachetul, construieste pachetul de trimitere si il
 * trimite catre clientii tcp folosind functia send_topic_to_subscribers
 */
void process_udp_packet(Topic_Register* t_register, SF_Packets_List* sf_pckt_list, int sockfd) {
    int buff_len = UDP_PACKET_LEN;
    struct sockaddr_in udp_sockaddr;
    UDP_Packet* recv_pkt = (UDP_Packet*) malloc(UDP_PACKET_LEN);
    DIE(!recv_pkt, "UDP_Packet malloc error");
    int wc;

    // primeste pachetul udp
    wc = recvfrom(sockfd, recv_pkt, UDP_PACKET_LEN, 0, (struct sockaddr*)&udp_sockaddr, &buff_len);
    DIE(wc < 0, "recvfrom error");

    // construieste pachetul de trimitere ce este format din pachetul UDP, ip-ul si port-ul
    // client-ului udp care a trimis pachetul
    // este trimis mai apoi acest pachet catre functia send_topic_to_subscribers
    TCP_Packet_Server* send_pkt = (TCP_Packet_Server*) malloc(TCP_PACKET_SERVER_LEN);
    DIE(!send_pkt, "TCP_Packet_Server malloc error");
    memcpy(&(send_pkt->packet), recv_pkt, UDP_PACKET_LEN);
    memcpy(&(send_pkt->udp_client_ip), inet_ntoa(udp_sockaddr.sin_addr), IP_MAX_LEN);
    memcpy(&(send_pkt->udp_client_port), &(udp_sockaddr.sin_port), sizeof(udp_sockaddr.sin_port));
    send_topic_to_subscribers(t_register, *send_pkt, sf_pckt_list);

    free(recv_pkt);
    free(send_pkt);
}

int main(int argc, char* argv[]) {
    FILE* fp = fopen("text.txt", "w");
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    DIE(argc != 2, "invalid number of arguments");
    int port = atoi(argv[1]);
    // include si port == 0 pt eroare la atoi
    // accept doar registered ports
    DIE(port < 1024 || port > 49151, "invalid port argument");

    fd_set* read_set = (fd_set*) malloc(sizeof(fd_set));
    DIE(!read_set, "set allocation");
    FD_ZERO(read_set);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket error");
    DIE(tcp_no_delay_opt(sockfd) < 0, "tcp_no_delay_opt error");

    /**
     * Creez socket-ul ce primeste cereri de conexiuni de la clientii tcp si il leg de port-ul
     * primit ca parametru, setez maxim-ul de cereri din coada la 10
     * Creez socket-ul prin care comunic cu clientii udp si il leg la acelasi port
     * Ambii socketi pot primi pachete de la orice adresa
     */
    struct sockaddr_in accept_addr;
    memset((char *) &accept_addr, 0, sizeof(accept_addr));
	accept_addr.sin_family = AF_INET;
	accept_addr.sin_port = htons(port);
	accept_addr.sin_addr.s_addr = INADDR_ANY;
    DIE(bind(sockfd, (struct sockaddr *)&accept_addr, sizeof(accept_addr)) < 0, "bind error");
    DIE(listen(sockfd, MAX_WAIT_CLIENTS) < 0, "listen error");
    int udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(bind(udp_sockfd, (struct sockaddr *)&accept_addr, sizeof(accept_addr)) < 0, "bind error");

    /**
     * Adaug cei doi socketi alaturi de file descriptor-ul stdin in set-ul pentru citire
     * Setez fdmax la maxim-ul dintre cei 3
     * Creez structurile ce contin date pe care le voi folosi cu capacitate initiala 10
     */
    FD_SET(udp_sockfd, read_set);
    FD_SET(sockfd, read_set);
    FD_SET(STDIN_FILENO, read_set);
    int fdmax = max(max(sockfd, udp_sockfd), STDIN_FILENO);
    Topic_Register* t_register = create_topic_register(INITIAL_CAPACITY);
    Client_Info_List* c_info_list = create_client_info_list(INITIAL_CAPACITY);
    SF_Packets_List* sf_pckt_list = create_sf_packets_list(INITIAL_CAPACITY);
    while (1) {
        fd_set* tmp_set = (fd_set*) malloc(sizeof(fd_set));
        DIE(!tmp_set, "temp set allocation");
        memcpy(tmp_set, read_set, sizeof(fd_set));

        /**
        * select asteapta dupa cereri de scriere de la file descriptorii din set-ul
        * read_set si cand primeste lasa doar file descriptorii de la care a primit in set
        * folosesc un set temporar pentru ca select distruge set-ul primit ca parametru
        * dupa ce se iese din select, trec prin toti file descriptorii posibili de la 0
        * pana la maximul dintre ele(pentru ca toate sunt >= 0 si logic mai mici decat maximul
        * dintre ele) si daca cumva face parte din set-ul ce contine doar file descriptorii
        * de la care se poate citi la momentul actual
        * atunci in functie de ce socket e se actioneaza in mod corespunzator
        */
        DIE(select(fdmax + 1, tmp_set, NULL, NULL, NULL) < 0, "select error");
        for (int sock_index = 0; sock_index <= fdmax; sock_index++) {
            if (FD_ISSET(sock_index, tmp_set)) {
                /* caz primire de cereri de a se conecta de la clienti tcp */
                if (sock_index == sockfd) {
                    socklen_t c_len = sizeof(struct sockaddr_in);
                    struct sockaddr_in c_sock_struct;
                    int new_client_sock = accept(sockfd, (struct sockaddr*)&c_sock_struct, &c_len);
                    DIE(new_client_sock < 0, "accept error");

                    // adaug socket-ul noului client in set-ul de read si actualizez fdmax
                    DIE(tcp_no_delay_opt(new_client_sock) < 0, "tcp_no_delay_opt error");
                    FD_SET(new_client_sock, read_set);
                    int old_fdmax = fdmax;
                    if (new_client_sock > fdmax) {
                        fdmax = new_client_sock;
                    }

                    // adaug socket-ul in set-ul temporar pentru a folosi select si a astepta
                    // sa vina pachet-ul cu id-ul acestuia
                    FD_SET(new_client_sock, tmp_set);
                    DIE(select(fdmax + 1, tmp_set, NULL, NULL, NULL) < 0, "select error");
                    int id_len = ID_MAX_LEN;
                    char* client_id = recv_all(new_client_sock, &id_len);
                    FD_CLR(new_client_sock, tmp_set);
                    DIE(id_len > ID_MAX_LEN, "Undefined id length");

                    /**
                    * caut client-ul dupa id sa vad daca este deja in baza de date
                    * daca este si e de asemenea conectat de pe alt device, atunci scot
                    * noul socket din set, aduc fdmax la starea anterioara si inchei
                    * conexiunea cu acesta dupa ce ii trimit un mesaj de exit(dupa
                    * ce il primeste ar trebui sa se inchida)
                    * daca a fost conectat anterior si s-a reconectat acum, atunci
                    * ii actualizez socket-ul si il setez ca fiind conectat, apoi
                    * trimit pachetele restante fata de acesta(pachetele cu topic
                    * fata de care avea SF activat)
                    */
                    TCP_Client* client = find_client_by_id(c_info_list, client_id);
                    char* c_ip = inet_ntoa(c_sock_struct.sin_addr);
                    uint16_t c_port = ntohs(c_sock_struct.sin_port);
                    if (client != NULL) {
                        if (client->is_connected == 1) {
                            printf("Client %s already connected.\n", client_id);
                            FD_CLR(new_client_sock, read_set);
                            fdmax = old_fdmax;
                            char buffer[EXIT_MSG_LEN] = "still connected";
                            send_all(new_client_sock, buffer, EXIT_MSG_LEN);
                            DIE(close(new_client_sock) < 0, "close new socket connection");
                            continue;
                        }

                        client->sockfd = new_client_sock;
                        client->is_connected = 1;
                        printf("New client %s connected from %s:%d.\n", client_id, c_ip, c_port);
                        send_remnant_SF_packets(t_register, sf_pckt_list, client);
                        continue;
                    }

                    // daca nu e in baza de date(lista de client), atunci il adaug
                    printf("New client %s connected from %s:%d.\n", client_id, c_ip, c_port);
                    add_client_to_info_list(c_info_list, new_client_sock, client_id);
                    continue;
                }

                /* caz primire comanda la stdin */
                if (sock_index == STDIN_FILENO) {
                    char buffer[5];
                    memset(buffer, 0, 5);
                    fgets(buffer, 5, stdin);
                    buffer[5] = '\0';

                    /**
                    * daca se primeste exit(singura comanda posibila), atunci pentru
                    * fiecare socket cu clientii tcp trimit un mesaj de exit dupa care acestia
                    * ar trebui sa se inchida si inchid conexiunea
                    * de asemenea inchid conexiunea pe socketul pentru primire de conexiuni
                    * de la clientii tcp si socket-ul pe care comunic cu clientii udp
                    * inchid in final server-ul cu exit() dupa ce dau free la structurile
                    * folosite pentru stocare de date
                    */
                    if (!strcmp(buffer, "exit")) {
                        for (int i = 0; i <= fdmax; i++) {
                            if (FD_ISSET(i, read_set)) {
                                // orice nu e socket de primire conexiuni, socket de comunicare cu
                                // clientii udp sau file descriptor de standard input este
                                // socket pentru clientii tcp
                                if(i != udp_sockfd && i != STDIN_FILENO && i != sockfd) {
                                    char buffer[EXIT_MSG_LEN] = "server shutdown";
                                    send_all(i, buffer, EXIT_MSG_LEN);
                                    DIE(close(i) < 0, "close connections on exit");
                                }
                            }
                        }

                        free_client_info_list(c_info_list);
                        free_topic_register(t_register);
                        free(read_set);
                        free(tmp_set);
                        DIE(close(sockfd) < 0, "close connections on exit");
                        DIE(close(udp_sockfd) < 0, "close connections on exit");
                        exit(1);
                    }

                    error_ignore(1, "Undefined server command");
                    continue;
                }

                /* caz primire pachete de la clientii udp */
                if (sock_index == udp_sockfd) {
                    process_udp_packet(t_register, sf_pckt_list, sock_index);
                    continue;
                }

                /* caz primire pachete de la clientii tcp */
                int buffer_len = TCP_PACKET_CLIENT_LEN;
                char* buffer = recv_all(sock_index, &buffer_len);
                DIE(!buffer, "buffer is NULL");
                DIE(buffer_len > TCP_PACKET_CLIENT_LEN, "tcp packet too big");

                // daca s-a primit mesajul "stop connection" inseamna ca clientul tcp
                // s-a inchis si astfel inchid conexiunea cu acesta
                if (!strcmp(buffer, "stop connection")) {
                    TCP_Client* disconnected_client = find_client_by_sock(c_info_list, sock_index);
                    DIE(!disconnected_client, "client not found");
                    printf("Client %s disconnected.\n", disconnected_client->id);
                    disconnected_client->is_connected = 0;
                    DIE(close(sock_index) < 0, "close disconnected client connection");
                    FD_CLR(sock_index, read_set);
                    continue;
                }

                // altfel s-a primit pachetul de la tcp si il copiez in structura aferenta
                // daca s-a primit comanda cu index-ul 0 inseamna ca este o cerere de subscribe
                // si il abonez la topic, altfel daca index-ul e 1 inseamna ca este o cerere de
                // unsubscribe si il dezabonez de la topic
                TCP_Packet_Client* recv_pkt = (TCP_Packet_Client*) malloc(TCP_PACKET_CLIENT_LEN);
                DIE(!recv_pkt, "malloc");
                memcpy(recv_pkt, buffer, TCP_PACKET_CLIENT_LEN);
                TCP_Client* client = find_client_by_sock(c_info_list, sock_index);
                DIE(!client, "client not found");
                if (ntohs(recv_pkt->command_index) == 0) {
                    subscribe_client_to_topic(t_register, recv_pkt->topic, client, recv_pkt->SF);
                } else if (ntohs(recv_pkt->command_index) == 1) {
                    unsubscribe_client_from_topic(t_register, recv_pkt->topic, client);
                } else {
                    error_ignore(1, "Undefined tcp client packet command index");
                }

                free(recv_pkt);
            }
        }

        free(tmp_set);
    }
}