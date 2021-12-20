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

typedef struct game {
    int player1;
    int player2;

    // 1 pentru player1, 2 pentru player2
    unsigned int whose_turn;
    char board[3][3];
    unsigned int number_of_free_boxes;

    // 1 pentru meci terminat, 0 pentru meci in derulare
    unsigned int status;
} Game;

Game* create_game(int player1, int player2) {
    Game* game = (Game*) malloc(sizeof(Game));
    game->player1 = player1;
    game->player2 = player2;
    game->whose_turn = 1;
    game->status = 0;
    game->number_of_free_boxes = 9;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            game->board[i][j] = 0;
        }
    }

    return game;
}

typedef struct gamelist {
    Game** games;
    unsigned int capacity;
    unsigned int size;
    int leftover_player;

    // 0 daca toti jucatorii au pereche, 1 daca 1 e pe dinafara
    unsigned int parity;
} GameList;

GameList* create_game_list() {
    GameList* game_list = (GameList*) malloc(sizeof(GameList));
    game_list->capacity = 10;
    game_list->games = (Game**) malloc(sizeof(Game*) * 10);
    game_list->size = 0;
    game_list->parity = 0;
    return game_list;
}

void add_player(GameList* game_list, int player) {
    if (game_list->parity == 0) {
        game_list->leftover_player = player;
        game_list->parity = 1;
        return;
    }

    Game* game = create_game(game_list->leftover_player, player);
    game_list->games[game_list->size] = game;
    game_list->size++;
    if (game_list->size == game_list->capacity) {
        game_list->capacity *= 2;
        game_list->games = realloc(game_list->games, game_list->capacity);
    }
}

char test_game_status(char board[3][3]) {
    for (int i = 0; i < 3; i++) {
        char consec_horizontal = board[i][0];
        char consec_vertical = board[0][i];
        for (int j = 1; j < 3; j++) {
            if (board[i][j] != consec_horizontal) {
                consec_horizontal = 0;
            }

            if (board[j][i] != consec_vertical) {
                consec_vertical = 0;
            }
        }

        if (consec_vertical != 0) {
            return consec_vertical;
        }

        if (consec_horizontal != 0) {
            return consec_horizontal;
        }
    }

    if (board[0][0] == board[1][1] && board[1][1] == board[2][2] && board[0][0] != 0) {
        return board[0][0];
    }

    if (board[2][0] == board[1][1] && board[1][1] == board[0][2] && board[2][0] != 0) {
        return board[2][0];
    }

    return 0;
}

/**
 * 1 pentru succes
 * -1 jocul nu a inceput inca
 * -2 nu e runda jucatorului
 * -3 casuta e deja completata
 * -4 pozitii invalide
 */
int complete_box(GameList* game_list, int player, int i, int j) {
    if (i < 0 || i > 3 || j < 0 || j > 3) {
        return -4;
    }

    for (int game_i = 0; game_i < game_list->size; game_i++) {
        char box_char = 0;
        Game* game = game_list->games[game_i];
        if (game->status == 1) {
            continue;
        }

        if (game->player1 == player) {
            if (game->whose_turn == 2) {
                return -2;
            }

            box_char = 'X';
            game->whose_turn = 2;
        }

        if (game->player2 == player) {
            if (game->whose_turn == 1) {
                return -2;
            }

            box_char = 'O';
            game->whose_turn = 1;
        }

        if (box_char == 0) {
            continue;
        }

        if (game->board[i][j] != 0) {
            return -3;
        }

        game->board[i][j] = box_char;
        game->number_of_free_boxes--;
        return 1;
    }

    // nu a gasit jocul in lista => nu a inceput inca
    return -1;
}

Game* find_game(GameList* game_list, int player) {
    for (int i = 0; i < game_list->size; i++) {
        Game* game = game_list->games[i];
        if ((game->player1 == player || game->player2 == player) && game->status == 0) {
            return game;
        }
    }

    return NULL;
}

void close_game(Game* game) {
    game->status = 1;
    DIE(close(game->player1) < 0, "close");
    DIE(close(game->player2) < 0, "close");
}

void notify_end_of_game(Game* game, int winner, int losser) {
    int two = 2;
    int three = 3;
    send(winner, &two, sizeof(int), 0);
    send(losser, &three, sizeof(int), 0);
    close_game(game);
}

void notify_draw(Game* game) {
    int four = 4;
    send(game->player1, &four, sizeof(int), 0);
    send(game->player2, &four, sizeof(int), 0);
    close_game(game);
}

void print_board(Game* game) {
    printf("+-----------------------------------------+\n");
    printf("Player1: %d; Player2: %d\n", game->player1, game->player2);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf("%c ", game->board[i][j]);
        }

        printf("\n");
    }

    printf("\n");
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
	int enable = 1;
	size_t len = sizeof(int);
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, len) == -1) {
	   perror("setsocketopt");
	   exit(1);
	}
	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;
    GameList* game_list = create_game_list();
	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
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
                    add_player(game_list, newsockfd);
					if (newsockfd > fdmax) { 
						fdmax = newsockfd;
					}

					printf("Noua conexiune de la %s, port %d, socket client %d\n",
							inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), newsockfd);
				} else {
					// s-au primit date pe unul din socketii de client,
					// asa ca serverul trebuie sa le receptioneze
					memset(buffer, 0, BUFLEN);
					n = recv(i, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					if (n == 0) {
						// conexiunea s-a inchis
						printf("Socket-ul client %d a inchis conexiunea\n", i);
						Game* game = find_game(game_list, i);
                        if (game != NULL) {
                            close_game(game);
                        }

						// se scoate din multimea de citire socketul inchis 
						FD_CLR(i, &read_fds);
					} else {
                        int box_status = complete_box(game_list, i, atoi(&buffer[0]), atoi(&buffer[2]));
						send(i, &box_status, sizeof(int), 0);
                        Game* game = find_game(game_list, i);
                        if (game == NULL) {
                            continue;
                        }

                        print_board(game);
                        if (game->number_of_free_boxes == 0) {
                            notify_draw(game);
                        }

                        char winner = test_game_status(game->board);
                        if (winner == 0) {
                            continue;
                        }

                        if (winner == 'X') {
                            notify_end_of_game(game, game->player1, game->player2);
                        }

                        if (winner == 'O') {
                            notify_end_of_game(game, game->player2, game->player1);
                        }
					}
				}
			}
		}
	}

	close(sockfd);

	return 0;
}