#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include "helpers.h"
#include "requests.h"

int main(int argc, char *argv[])
{
    char *message;
    char *response;
    int sockfd;

        
    // Ex 1: GET dummy from main server
    sockfd = open_connection("34.118.48.238", 8080, AF_INET, SOCK_STREAM, 0);
    message = compute_get_request("34.118.48.238", "/api/v1/dummy", NULL, NULL, 0);
    send_to_server(sockfd, message);
    printf("EX1:\n");
    printf("%s\n", message);
    printf("%s\n", receive_from_server(sockfd));
    close(sockfd);

    // Ex 2: POST dummy and print response from main server
    sockfd = open_connection("34.118.48.238", 8080, AF_INET, SOCK_STREAM, 0);
    char** body_data = (char**)malloc(sizeof(char*) * 2);
    body_data[0] = (char*)malloc(sizeof(char) * 50);
    body_data[1] = (char*)malloc(sizeof(char) * 50);
    strcpy(body_data[0], "dummy1=ceva+da");
    strcpy(body_data[1], "dummy2=altceva");

    char** cookies = (char**)malloc(sizeof(char*) * 2);
    cookies[0] = (char*)malloc(sizeof(char) * 50);
    cookies[1] = (char*)malloc(sizeof(char) * 50);
    strcpy(cookies[0], "a=5");
    strcpy(cookies[1], "b=7");

    message = compute_post_request("34.118.48.238", "/api/v1/dummy", "application/x-www-form-urlencoded", body_data, 2, cookies, 2);
    printf("\nEX2:\n");
    printf("\n===%s\n===\n", message);
    send_to_server(sockfd, message);
    printf("%s\n", receive_from_server(sockfd));
    close(sockfd);

    // Ex 3: Login into main server
    sockfd = open_connection("34.118.48.238", 8080, AF_INET, SOCK_STREAM, 0);
    printf("\nEX3:\n");
    strcpy(body_data[0], "username=student");
    strcpy(body_data[1], "password=student");
    message = compute_post_request("34.118.48.238", "/api/v1/auth/login", "application/x-www-form-urlencoded", body_data, 2, cookies, 2);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    printf("%s\n", message);
    printf("%s\n", response);
    close(sockfd);

    // Ex 4: GET weather key from main server
    sockfd = open_connection("34.118.48.238", 8080, AF_INET, SOCK_STREAM, 0);
    printf("\nEX4:\n");
    cookies = (char**)realloc(cookies, sizeof(char*) * 3);
    cookies[0] = (char*)realloc(cookies[0], sizeof(char) * 100);
    cookies[2] = (char*)malloc(sizeof(char) * 50);

    // Citesc primul cookie care e mereu diferit de la tastatura
    printf("Va rog introduceti connect-sid-ul de mai sus: ");
    fgets(cookies[0], 100, stdin);

    // ca sa scap de newline
    cookies[0][strlen(cookies[0]) - 1] = '\0';
    strcpy(cookies[1], "Path=/");
    strcpy(cookies[2], "HttpOnly");
    message = compute_get_request("34.118.48.238", "/api/v1/weather/key", NULL, cookies, 3);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    printf("%s\n", message);
    printf("%s\n", response);
    close(sockfd);

    // Ex 5: Logout from main server
    sockfd = open_connection("34.118.48.238", 8080, AF_INET, SOCK_STREAM, 0);
    printf("\nEX5:\n");
    message = compute_get_request("34.118.48.238", "/api/v1/auth/logout", NULL, cookies, 3);
    send_to_server(sockfd, message);
    response = receive_from_server(sockfd);
    printf("%s\n", message);
    printf("%s\n", response);
    close(sockfd);

    // BONUS: make the main server return "Already logged in!"

    // free the allocated data at the end!
    free(body_data[0]);
    free(body_data[1]);
    free(body_data);
    free(cookies[0]);
    free(cookies[1]);
    free(cookies[2]);
    free(cookies);

    return 0;
}
