#include "helpers.h"

void send_all(int socket, const char* buffer, size_t len) {
    size_t bytes_remaining = len;
    size_t bytes_sent = 0;
    while(bytes_remaining > 0) {
        int wc = send(socket, &buffer[bytes_sent], bytes_remaining, 0);
        DIE(wc <= 0, "send");
        bytes_remaining -= wc;
        bytes_sent += wc;
    }
}