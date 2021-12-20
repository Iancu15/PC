#ifndef _UTILS_
#define _UTILS_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "helpers.h"
#include "parson.h"

// lungimea campurilor introduse prin prompturi de la tastatura
#define FIELD_LEN 50

// ip-ul si portul clientului
#define HOST "34.118.48.238"
#define PORT 8080

char* get_field(const char* field_name, int size);
int get_numeral_field(const char* field_name, const char* error_msg);

void register_func(char* username, char* password);
char* login(char* username, char* password);
char* enter_library(char* cookies);
void get_books(char* jwt_token);
void get_book(char* jwt_token, unsigned int id);
char* get_book_json(char* title, char* author, char* genre, char* publisher, unsigned int page_count);
void add_book(char* jwt_token, char* title, char* author, char* genre, char* publisher, unsigned int page_count);
void delete_book(char* jwt_token, unsigned int id);
int logout(char* cookies);

char* generate_post_request(char* url, char* payload, char* cookies, char* jwt_token);
char* generate_get_request(char* url, char* cookies, char* jwt_token, int id);
char* generate_delete_request(char *url, char* jwt_token, int id);

char* authentication_json(char* username, char* password);
char* access_server(char* message);
char* get_nth_token_from_str(char* str, char* delim, int n);
char* get_nth_line(char* str, int n);
char* get_status(char* response);
int is_bad_request(char* response);
int is_ok(char* response);
void print_error(char* response, int error_line);

#endif