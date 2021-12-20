#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "link_emulator/lib.h"

#define HOST "127.0.0.1"
#define PORT 10000


int main(int argc, char** argv){
  init(HOST,PORT);
  msg t;
  msg ACK;

  // Trimit numele fisierului
  t.len = strlen(argv[1]);
  sprintf(t.payload, "%s", argv[1]);
  send_message(&t);

    // Verifica confirmarea
  if (recv_message(&ACK) < 0){
    perror("Receive message");
    exit(-1);
  }

  FILE *filePtr;
  filePtr = fopen(argv[1], "rb");

  while (1) {
    int rc = fread(t.payload, 1, MAX_LEN, filePtr);

    if (rc < 0) {
      perror("Read error:");
      exit(-1);
    }

    t.len = rc;
    send_message(&t);

    // Verifica confirmarea
    if (recv_message(&ACK) < 0){
      perror("Receive message");
      exit(-1);
    }

    // Daca citeste ultimul
    // Asta verifica pratic ca e in intervalul [0, MAX_LEN)
    // pentru ca daca a ajuns pana aici inseamna ca nu s-a
    // activat rc < 0
    if (rc < MAX_LEN) {
      break;
    }
  }

  // Trimite mesajul de oprire
  t.len = 0;
  send_message(&t);

  // Verifica confirmarea
  if (recv_message(&ACK) < 0){
    perror("Receive message");
    exit(-1);
  }

  fclose(filePtr);

  return 0;
}
