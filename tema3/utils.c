#include "utils.h"

/**
 * Afiseaza un prompt pentru utilizator pentru citirea unui camp.
 * @param field_name    sirul de caractere pentru prompt-ul ce va aparea utilizatorului
 * @param size          lungimea campului ce urmeaza sa fie introdus de utilizator
 * @return              sirul introdus de utilizator
 */
char* get_field(const char* field_name, int size) {
    char* field = (char*) malloc(sizeof(char) * size);
    if (!field)
        error("malloc");

    printf("%s", field_name);
    fgets(field, size, stdin);
    // scapa de newline
    field[strlen(field) - 1] = '\0';
    return field;
}

/**
 * Afiseaza un prompt pentru utilizator pentru citirea unui camp numeric.
 * @param field_name    sirul de caractere pentru prompt-ul ce va aparea utilizatorului
 * @param error_msg     mesajul de eroare in cazul in care sirul introdus nu e un numar natural
 * @return              echivalentul numeric al sirului introdus, -1 daca e invalida intrarea
 */
int get_numeral_field(const char* field_name, const char* error_msg) {
    char* str = get_field(field_name, FIELD_LEN);
    int field = atoi(str);

    // atoi da 0 in caz de esec, asa ca pentru ca id-ul 0 sa poata sa fie valid, atunci a trebuit sa il exclud
    // din cazurile esuate precizand ca sirul trebuie sa fie diferit de "0" pentru ca valoarea 0 sa indice o eroare
    if (field < 0 || (field == 0 && !(str[0] == '0' && strlen(str) == 1))) {
        puts(error_msg);
        return -1;
    }

    return field;
}

/**
 * Genereaza o cerere de tip POST.
 * @param url           url-ul server-ului catre care se trimite cererea
 * @param payload       mesajul ce vrea sa fie postat, NULL daca nu vrea sa trimita niciun mesaj
 * @param cookies       cookie-uri daca are, NULL in caz contrar
 * @param jwt_token     codul de autorizare daca are, NULL in caz contrar
 * @return              sirul ce contine intreaga cerere POST
 */
char* generate_post_request(char* url, char* payload, char* cookies, char* jwt_token) {
    char* message = (char*) calloc(BUFLEN, sizeof(char));
    char *line = (char*) malloc(sizeof(char) * LINELEN);
    sprintf(line, "POST %s HTTP/1.1\r\n", url);
    strcat(message, line);
    sprintf(line, "Host: %s\r\n", HOST);
    strcat(message, line);
    if (payload != NULL) {
        sprintf(line, "Content-Length: %ld\r\n", strlen(payload));
        strcat(message, line);
    } else {
        strcat(message, "Content-Length: 0\r\n");
    }

    strcat(message, "Content-Type: application/json\r\n");
    if (cookies != NULL) {
        // sirul de cookie-uri primit la intrare contine si header-ul
        sprintf(line, "%s\r\n", cookies);
        strcat(message, line);
    }

    if (jwt_token != NULL) {
        sprintf(line, "Authorization: Bearer %s\r\n", jwt_token);
        strcat(message, line);
    }

    strcat(message, "\r\n");
    if (payload != NULL) {
        strcat(message, payload);
    }

    free(line);
    return message;
}

/**
 * Genereaza o cerere de tip GET.
 * @param url           url-ul server-ului catre care se trimite cererea
 * @param cookies       cookie-uri daca are, NULL in caz contrar
 * @param jwt_token     codul de autorizare daca are, NULL in caz contrar
 * @param id            id-ul pentru a fi adaugat drept query daca este necesar, -1 in caz contrar
 * @return              sirul ce contine intreaga cerere GET
 */
char* generate_get_request(char* url, char* cookies, char* jwt_token, int id) {
    char* message = (char*) calloc(BUFLEN, sizeof(char));
    char *line = (char*) malloc(sizeof(char) * LINELEN);
    if (id == -1) {
        sprintf(line, "GET %s HTTP/1.1\r\n", url);
        strcat(message, line);
    } else {
        sprintf(line, "GET %s/%d HTTP/1.1\r\n", url, id);
        strcat(message, line);
    }

    sprintf(line, "Host: %s\r\n", HOST);
    strcat(message, line);
    if (cookies != NULL) {
        // sirul de cookie-uri primit la intrare contine si header-ul
        sprintf(line, "%s\r\n", cookies);
        strcat(message, line);
    }

    if (jwt_token != NULL) {
        sprintf(line, "Authorization: Bearer %s\r\n", jwt_token);
        strcat(message, line);
    }

    strcat(message, "\r\n");
    free(line);
    return message;
}

/**
 * Genereaza o cerere de tip DELETE.
 * @param url           url-ul server-ului catre care se trimite cererea
 * @param jwt_token     codul de autorizare daca are, NULL in caz contrar
 * @param id            id-ul pentru a fi adaugat drept query daca este necesar, -1 in caz contrar
 * @return              sirul ce contine intreaga cerere DELETE
 */
char* generate_delete_request(char *url, char* jwt_token, int id) {
    char* message = (char*) calloc(BUFLEN, sizeof(char));
    char *line = (char*) malloc(sizeof(char) * LINELEN);
    if (id == -1) {
        sprintf(line, "DELETE %s HTTP/1.1\r\n", url);
        strcat(message, line);
    } else {
        sprintf(line, "DELETE %s/%d HTTP/1.1\r\n", url, id);
        strcat(message, line);
    }

    sprintf(line, "Host: %s\r\n", HOST);
    strcat(message, line);

    if (jwt_token != NULL) {
        sprintf(line, "Authorization: Bearer %s\r\n", jwt_token);
        strcat(message, line);
    }

    strcat(message, "\r\n");
    free(line);
    return message;
}

/**
 * Creeaza JSON-ul de autentificare ce contine campurile username si password
 * @param username      numele utilizatorului
 * @param password      parola utilizatorului
 * @return              json-ul in format text
 */
char* authentication_json(char* username, char* password) {
    JSON_Value* json_value = json_value_init_object();
    JSON_Object* json_obj = json_value_get_object(json_value);
    json_object_set_string(json_obj, "username", username);
    json_object_set_string(json_obj, "password", password);
    free(username);
    free(password);
    return json_serialize_to_string(json_value);
}

/**
 * Deschide conexiunea catre server, trimite mesajul, primeste raspunsul si inchide conexiunea.
 * @param message       mesajul de cerere ce urmeaza a fi trimis
 * @return              mesajul de raspuns al server-ului
 */
char* access_server(char* message) {
    int sockfd = open_connection(HOST, PORT, AF_INET, SOCK_STREAM, 0);
    send_to_server(sockfd, message);
    char* response = receive_from_server(sockfd);
    if (!strcmp(get_status(response), "429")) {
        printf("Too many requests, please try again later!\n");
    }

    if (close(sockfd) < 0) {
        error("Couldn't close connection");
    }

    return response;
}

/**
 * Intoarce al n-lea token(format prin impartirea dupa delimitare) dintr-un sir.
 * @param str       sirul ce urmeaza a fi procesat
 * @param delim     delimitatorul dintre token-uri
 * @param n         index-ul token-ului cautat(se incepe de la 1)
 * @return          token-ul cautat
 */
char* get_nth_token_from_str(char* str, char* delim, int n) {
    char* token = strtok(str, delim);
    int token_index = 1;
    while (token_index < n) {
        token = strtok(NULL, delim);
        token_index++;
    }

    return token;
}

/**
 * Intoarcea a n-a linie dintr-un sir.
 * @param str   sirul ce urmeaza a fi procesat
 * @param n     index-ul liniei(de la 1)
 * @return      linia cautata
 */
char* get_nth_line(char* str, int n) {
    return get_nth_token_from_str(str, "\n", n);
}

/**
 * Intoarce statusul(ex: 404) aferent raspunsului dat de server
 * @param response      raspunsul dat de server
 * @return              statusul in format text
 */
char* get_status(char* response) {
    // prima linie din raspuns este de forma HTTP/1.1 %status% %raspuns_aditional%, asa ca am nevoie de al 2-lea
    // token delimitat de " " si anume %status%
    return get_nth_token_from_str(strdup(response), " ", 2);
}

/**
 * Verifica daca raspunsul dat de server este afirmativ (este de forma 2XX).
 * @param response      raspunsul serverului
 * @return              1 daca raspunsul e afirmativ, 0 in caz contrar
 */
int is_ok(char* response) {
    return get_status(response)[0] == '2';
}

/**
 * Inregistreaza utilizatorul cu credentialele primite. Daca raspunsul dat de server este afirmativ se afiseaza
 * "Registered successfuly!", in caz contrar se afiseaza eroarea data de server care se afla in json-ul de la linia
 * 19 din raspuns.
 * @param username      numele utilizatorului
 * @param password      parola utilizatorului
 */
void register_func(char* username, char* password) {
    char* payload = authentication_json(username, password);
    char* message = generate_post_request("/api/v1/tema/auth/register", payload, NULL, NULL);
    char* response = access_server(message);
    if (is_ok(response)) {
        printf("Registered successfuly!\n");
    } else {
        print_error(response, 19);
    }

    free(response);
    free(message);
}

/**
 * Logheaza utilizatorul cu credentialele primite. Daca raspunsul oferit de server este afirmativ se afiseaza
 * mesajul "Authenticated successfuly!" si se extrage token-ul din mesaj, in caz contrar se afiseaza mesajul de
 * eroare de la linia 16 din raspuns.
 * @param username      numele utilizatorului
 * @param password      parola utilizatorului
 * @return              cookie-ul de conectare(connect.sid) cu tot cu header daca raspunsul a fost afirmativ, NULL
 *                      in caz contrar
 */
char* login(char* username, char* password) {
    char* payload = authentication_json(username, password);
    char* message = generate_post_request("/api/v1/tema/auth/login", payload, NULL, NULL);
    char* response = access_server(message);
    char* cookies = NULL;
    if (is_ok(response)) {
        printf("Authenticated successfuly!\n");

        // cookie-ul se afla la linia 9 din raspuns, extrag linia si incrementez pointer-ul de char-uri cu 4 pentru
        // a scapa de primele 4 caractere "Set-"
        // scap de celelalte 2 cookie-uri, cea de path si cea de httponly, si pastrez doar sirul final de forma
        // "Cookie: connect.sid=%connect.sid%"
        cookies = get_nth_line(response, 9);
        cookies += 4;
        cookies = get_nth_token_from_str(cookies, ";", 1);
    } else {
        print_error(response, 16);
    }

    free(response);
    free(message);
    return cookies;
}

/**
 * Ofera acces utilizatorului in biblioteca. Daca raspunsul oferit de server este afirmativ, atunci se afiseaza
 * mesajul "Entered the library successfuly!" si se extrage token-ul jwt din json-ul aflat la linia 16 in raspuns,
 * in caz contrar la linia 16 se va afla json-ul cu eroarea aferenta cererii pe care o afisez la stdin.
 * @param auth_cookies      sirul cu cookie-ul de autentificare de forma "Cookie: connect.sid=%connect.sid%"
 * @return                  token-ul jwt cu care utilizatorul sa se autorizeze de fiecare data cand foloseste
 *                          de facilitati din biblioteca
 */
char* enter_library(char* auth_cookies) {
    if (auth_cookies == NULL) {
        printf("You aren't logged in!\n");
        return NULL;
    }

    char* message = generate_get_request("/api/v1/tema/library/access", auth_cookies, NULL, -1);
    char* response = access_server(message);
    if (is_ok(response)) {
        printf("Entered the library successfuly!\n");
    } else {
        print_error(response, 16);
        free(response);
        free(message);
        return NULL;
    }

    JSON_Object* jwt_token_obj = json_object(json_parse_string(get_nth_line(response, 16)));
    free(response);
    free(message);
    return (char*) json_object_get_string(jwt_token_obj, "token");
}

/**
 * Trimite o cerere de accesare a cartilor din biblioteca. Daca raspunsul este afirmativ, atunci pe linia 16 se va
 * afla json array-ul cu cartile cautate. Se transpune treptat text-ul intr-o structura JSON_Array pe care o parcurg
 * si pentru fiecare carte din vector-ul respectiv afisez id-ul si titlul. In caz contrar afisez mesajul de eroare
 * de la linia 16.
 * @param jwt_token         codul de autorizare in biblioteca
 */
void get_books(char* jwt_token) {
    if (jwt_token == NULL) {
        printf("You don't have access to the library!\n");
        return;
    }

    char* message = generate_get_request("/api/v1/tema/library/books", NULL, jwt_token, -1);
    char* response = access_server(message);
    if (is_ok(response)) {
        JSON_Value* books = json_parse_string(get_nth_line(response, 16));
        JSON_Array* books_arr = json_value_get_array(books);
        size_t number_of_books = json_array_get_count(books_arr);
        if (number_of_books == 0) {
            printf("There are no books!\n");
        }

        for (size_t i = 0; i < json_array_get_count(books_arr); i++) {
            JSON_Object* book = json_array_get_object(books_arr, i);
            printf("%d ", (int) json_object_get_number(book, "id"));
            printf("%s\n", json_object_get_string(book, "title"));
        }
    } else {
        print_error(response, 16);
    }

    free(message);
    free(response);
}

/**
 * Trimite o cerere de GET catre server prin care cere detaliile cartii cu un anume id. Daca raspunsul este afirmativ,
 * atunci parseaza json-ul aflat la linia 16 in raspuns si afiseaza fiecare camp din acesta la stdin, in caz contrar
 * afiseaza mesajul de eroare aflat la linia 16.
 * @param jwt_token     cerere de autorizare in biblioteca
 * @param id            id-ul cartii cautate
 */
void get_book(char* jwt_token, unsigned int id) {
    char* message = generate_get_request("/api/v1/tema/library/books", NULL, jwt_token, id);
    char* response = access_server(message);

    if (is_ok(response)) {
        JSON_Value* book_val = json_parse_string(get_nth_line(response, 16));
        JSON_Object* book = json_array_get_object(json_value_get_array(book_val), 0);
        printf("Title: %s\n", json_object_get_string(book, "title"));
        printf("Author: %s\n", json_object_get_string(book, "author"));
        printf("Publisher: %s\n", json_object_get_string(book, "publisher"));
        printf("Genre: %s\n", json_object_get_string(book, "genre"));
        printf("Page Count: %d\n", (int) json_object_get_number(book, "page_count"));
    } else {
        print_error(response, 16);
    }

    free(message);
    free(response);
}

/**
 * Formeaza un json ce contine cate un camp pentru fiecare din parametrii si adauga valoarea acestora.
 * @param title         titlul cartii
 * @param author        autorul cartii
 * @param genre         genul cartii
 * @param publisher     editura cartii
 * @param page_count    numarul de pagini din carte
 * @return              json-ul in format text
 */
char* get_book_json(char* title, char* author, char* genre, char* publisher, unsigned int page_count) {
    JSON_Value* json_value = json_value_init_object();
    JSON_Object* json_obj = json_value_get_object(json_value);
    json_object_set_string(json_obj, "title", title);
    json_object_set_string(json_obj, "author", author);
    json_object_set_string(json_obj, "genre", genre);
    json_object_set_number(json_obj, "page_count", page_count);
    json_object_set_string(json_obj, "publisher", publisher);

    free(title);
    free(author);
    free(genre);
    free(publisher);
    return json_serialize_to_string(json_value);
}

/**
 * Trimite o cerere POST la server prin care adauga o carte in biblioteca. Daca raspunsul este afirmativ, atunci
 * se afiseaza mesajul "The book was added!", in caz contrar se afiseaza eroarea de la linia 19.
 * @param jwt_token     codul de autorizare
 * @param title         titlul cartii
 * @param author        autorul cartii
 * @param genre         genul cartii
 * @param publisher     editura cartii
 * @param page_count    numarul de pagini
 */
void add_book(char* jwt_token, char* title, char* author, char* genre, char* publisher, unsigned int page_count) {
    char* payload = get_book_json(title, author, genre, publisher, page_count);
    char* message = generate_post_request("/api/v1/tema/library/books", payload, NULL, jwt_token);
    char* response = access_server(message);

    if (is_ok(response)) {
        printf("The book was added!\n");
    } else {
        print_error(response, 19);
    }

    free(message);
    free(response);
    free(payload);
}

/**
 * Trimite o cerere de DELETE a cartii, cu un anume id, din biblioteca. Daca raspunsul este afirmativ, afiseaza
 * "The book was deleted!", in caz contrar afiseaza mesajul de eroare de la linia 16.
 * @param jwt_token     codul de autorizare in biblioteca
 * @param id            id-ul cartii ce doreste a fi stearsa
 */
void delete_book(char* jwt_token, unsigned int id) {
    char* message = generate_delete_request("/api/v1/tema/library/books", jwt_token, id);
    char* response = access_server(message);

    if (is_ok(response)) {
        printf("The book was deleted!\n");
    } else {
        print_error(response, 16);
    }

    free(message);
    free(response);
}

/**
 * Delogheaza utilizatorul. Daca raspunsul este afirmativ se afiseaza "You logged out successfuly!", in caz contrar
 * se afiseaza mesajul de eroare de la linia 16.
 * @param auth_cookies
 * @return intoarce 1 daca delogarea s-a efectuat cu succes, -1 in caz contrar
 */
int logout(char* auth_cookies) {
    if (auth_cookies == NULL) {
        printf("You aren't logged in!\n");
        return -1;
    }

    char* message = generate_get_request("/api/v1/tema/auth/logout", auth_cookies, NULL, -1);
    char* response = access_server(message);
    int ok;
    if (is_ok(response)) {
        printf("You logged out successfuly!\n");
        ok = 1;
    } else {
        print_error(response, 16);
        ok = -1;
    }

    free(response);
    free(message);
    return ok;
}

/**
 * Afiseaza mesajul de eroare din raspuns de la linia precizata. Considera ca apelantul stie ce face si ia ca atare
 * linia de eroare data la parametru fara sa faca alte verificari.
 * @param response      raspunsul cererii date de server
 * @param error_line    linia la care se afla eroarea
 */
void print_error(char* response, int error_line) {
    // parsez json-ul erorii si afisez campul corespondent
    JSON_Object* error_obj = json_object(json_parse_string(get_nth_line(response, error_line)));
    puts(json_object_get_string(error_obj, "error"));
}
