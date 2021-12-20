#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

void send_message(int sockfd, const char* message) {
	send_all(sockfd, message, MSG_LEN);
}

typedef struct client {
	// numele clientului
	char id[ID_MAX_LEN];
	int sockfd;

	// 1 da, 0 nu
	unsigned int is_a_candidate;
	unsigned int number_of_votes;

	// al catalea candidat e, -1 in caz ca nu e candidat
	int candidate_index;

	// 1 da, 0 nu
	unsigned int voted;
	unsigned int connected;
} Client;

typedef struct clientinfo {
    Client** clients;
    unsigned int size;
    unsigned int capacity;
	unsigned int next_candidate_index;
} Client_Info_List;

Client_Info_List* create_client_info_list(unsigned int initial_capacity) {
    Client_Info_List* c_info_list = (Client_Info_List*) malloc(sizeof(Client_Info_List));
    DIE(!c_info_list, "malloc");
    c_info_list->clients = (Client**) malloc(sizeof(Client*) * initial_capacity);
    DIE(!c_info_list->clients, "malloc");
    c_info_list->capacity = initial_capacity;
    c_info_list->size = 0;
	c_info_list->next_candidate_index = 0;

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
Client* find_client_by_id(Client_Info_List* c_info_list, char* id) {
    for (int i = 0; i < c_info_list->size; i++) {
        if (!strcmp(c_info_list->clients[i]->id, id)) {
            return c_info_list->clients[i];
        }
    }

    return NULL;
}

/**
 * Adauga un client in lista de clienti
 * Intoarce -1 daca e deja conectat, 1 in rest
 */
int add_client_to_info_list(Client_Info_List* c_info_list, int sockfd, char* id) {
    Client* client = find_client_by_id(c_info_list, id);
	// clientul e deja conectat, se inchide acesta
    if (client != NULL) {
		if (client->connected) {
			close(sockfd);
			return -1;
		}

		client->sockfd = sockfd;
		return 1;
	}

    c_info_list->clients[c_info_list->size++] = (Client*) malloc(sizeof(Client));
    client = c_info_list->clients[c_info_list->size - 1];
    DIE(!client, "malloc");
    client->sockfd = sockfd;
    memcpy(&(client->id), id, ID_MAX_LEN);

    client->is_a_candidate = 0;
    client->number_of_votes = 0;
	client->voted = 0;
	client->candidate_index = -1;
	client->connected = 1;
    if (c_info_list->size == c_info_list->capacity) {
        c_info_list->capacity *= 2;
        size_t new_size = sizeof(Client) * c_info_list->capacity;
        c_info_list->clients = realloc(c_info_list->clients, new_size);
        DIE(!c_info_list->clients, "c_info_list realloc error");
    }

	return 1;
}

/**
 * Cauta un client dupa socket-ul pe care sever-ul comunica cu el
 */
Client* find_client_by_sock(Client_Info_List* c_info_list, int sockfd) {
    for (int i = 0; i < c_info_list->size; i++) {
        if (c_info_list->clients[i]->sockfd == sockfd) {
            return c_info_list->clients[i];
        }
    }

    return NULL;
}

Client* find_client_by_index(Client_Info_List* c_info_list, int index) {
    for (int i = 0; i < c_info_list->size; i++) {
		Client* client = c_info_list->clients[i];
        if (client->is_a_candidate == 1 && client->candidate_index == index) {
            return c_info_list->clients[i];
        }
    }

    return NULL;
}

void add_candidate(Client_Info_List* c_list, int sockfd) {
	Client* client = find_client_by_sock(c_list, sockfd);
	if (client == NULL) {
		DIE(1, "Client not found 1!\n");
		return;
	}

	client->is_a_candidate = 1;
	client->candidate_index = c_list->next_candidate_index;
	c_list->next_candidate_index++;
}

void send_list(Client_Info_List* c_list, int sockfd) {
	int index = 0;
	while (1) {
		Client* candidate = find_client_by_index(c_list, index);
		if (candidate == NULL) {
			if (index == 0) {
				send_all(sockfd, "No candidates in the race!\n", MSG_LEN);
			} else {
				send_all(sockfd, "\n", 2);
			}

			break;
		}

		// inca 4 pentru int si 1 pt spatiu
		char buffer[ID_MAX_LEN + 5];
		DIE(sprintf(buffer, "%s %d ", candidate->id, candidate->number_of_votes) < 0, "sprintf");
		send_all(sockfd, buffer, strlen(buffer));
		index++;
	}
}

void votefor(Client_Info_List* c_list, int sockfd, int number) {
	Client* client = find_client_by_sock(c_list, sockfd);
	if (client == NULL) {
		DIE(1, "Client not found 2!\n");
		return;
	}

	if (client->voted == 1) {
		send_message(sockfd, "You already voted!\n");
		return;
	}

	Client* candidate = find_client_by_index(c_list, number);
	if (candidate == NULL) {
		send_message(sockfd, "There's no such candidate!\n");
		return;
	}

	client->voted = 1;
	candidate->number_of_votes++;
}

void process_message(Client_Info_List* c_list, int sockfd, char* buffer) {
	if (strncmp(buffer, "candidez", 8) == 0) {
		add_candidate(c_list, sockfd);
		return;
	}

	if (strncmp(buffer, "show", 4) == 0) {
		send_list(c_list, sockfd);
		return;
	}

	char* token = strtok(buffer, " \n");
	if (strcmp(token, "votefor") != 0) {
		send_message(sockfd, "Invalid command!\n");
		return;
	}

	token = strtok(NULL, " \n");
	int number = atoi(token);
	if (number < 0 || (number == 0 && !(token[0] == '0' && strlen(token) == 1))) {
		send_message(sockfd, "Invalid candidate number!\n");
        return;
    }

	votefor(c_list, sockfd, number);
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (read_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;
	Client_Info_List* c_list = create_client_info_list(10);
	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
					// pe care serverul o accepta
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// se adauga noul socket intors de accept() la multimea descriptorilor de citire
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) { 
						fdmax = newsockfd;
					}
					char client_id[ID_MAX_LEN];
					recv(newsockfd, client_id, ID_MAX_LEN, 0);

					printf("New connection from %s\n",
							client_id);
					int is_ok = add_client_to_info_list(c_list, newsockfd, client_id);
					if (is_ok == -1) {
						FD_CLR(newsockfd, &read_fds);
					}
				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						// conexiunea s-a inchis
						Client* client = find_client_by_sock(c_list, i);
						printf("Client %s closed the connection\n", client->id);
						client->connected = 0;
						close(i);
						
						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
					} else {
						process_message(c_list, i, buffer);
					}
				}
			}
		}
	}

	close(sockfd);

	return 0;
}
