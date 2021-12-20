#include "utils.h"

int main() {
    char* auth_cookies = NULL;
    char* jwt_token = NULL;
    while (1) {
        char* command = (char*) malloc(sizeof(char) * 20);
        if (!command)
            error("malloc");

        fgets(command, 20, stdin);
        // scap de newline
        command[strlen(command) - 1] = '\0';
        if (!strcmp(command, "register")) {
            char* username = get_field("username=", FIELD_LEN);
            char* password = get_field("password=", FIELD_LEN);
            register_func(username, password);
            continue;
        }

        if (!strcmp(command, "login")) {
            if (auth_cookies != NULL) {
                printf("You are already logged in!\n");
                continue;
            }

            // dupa logare ar trebui sa am in auth_cookies header-ul cu cookie-ul printr-un sir de forma
            // "Cookie: connect.sid=%connect.sid%", daca logarea nu s-a efectuat cu succes acesta ramane NULL
            char* username = get_field("username=", FIELD_LEN);
            char* password = get_field("password=", FIELD_LEN);
            auth_cookies = login(username, password);
            continue;
        }

        if (!strcmp(command, "enter_library")) {
            if (jwt_token != NULL) {
                printf("You are already inside the library!\n");
                continue;
            }

            // dupa logare ar trebui sa am in jwt_token codul de autorizare in biblioteca, daca n-am putut accesa
            // biblioteca, atunci acesta ar trebui sa ramana NULL
            jwt_token = enter_library(auth_cookies);
            continue;
        }

        if (!strcmp(command, "get_books")) {
            get_books(jwt_token);
            continue;
        }

        if (!strcmp(command, "get_book")) {
            if (jwt_token == NULL) {
                printf("You don't have access to the library!\n");
                continue;
            }

            unsigned int id = get_numeral_field("id=", "Invalid id!");
            if (id != -1) {
                get_book(jwt_token, id);
            }

            continue;
        }

        if (!strcmp(command, "add_book")) {
            if (jwt_token == NULL) {
                printf("You don't have access to the library!\n");
                continue;
            }

            char* title = get_field("title=", FIELD_LEN);
            char* author = get_field("author=", FIELD_LEN);
            char* genre = get_field("genre=", FIELD_LEN);
            char* publisher = get_field("publisher=", FIELD_LEN);
            unsigned int page_count = get_numeral_field("page_count=", "Invalid page count!");
            if (page_count != -1) {
                add_book(jwt_token, title, author, genre, publisher, page_count);
            }

            continue;
        }

        if (!strcmp(command, "delete_book")) {
            if (jwt_token == NULL) {
                printf("You don't have access to the library!\n");
                continue;
            }

            unsigned int id = get_numeral_field("id=", "Invalid id!");
            if (id != -1) {
                delete_book(jwt_token, id);
            }

            continue;
        }

        if (!strcmp(command, "logout")) {
            int ok = logout(auth_cookies);
            if (ok == 1) {
                auth_cookies = NULL;
                jwt_token = NULL;
            }

            continue;
        }

        if (!strcmp(command, "exit")) {
            logout(auth_cookies);
            exit(1);
        }

        printf("INVALID COMMAND!\n");
        free(command);
    }
}
