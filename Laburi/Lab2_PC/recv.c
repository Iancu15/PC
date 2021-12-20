#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "link_emulator/lib.h"

#define HOST "127.0.0.1"
#define PORT 10001


int main(int argc,char** argv){
  msg r;
  msg ACK;

  sprintf(ACK.payload, "%s", "ACK");
  ACK.len = strlen(ACK.payload) + 1; 

  init(HOST,PORT);

  // Eroare daca n-a primit numele fisierului
  if (recv_message(&r) < 0){
    perror("Receive message");
    exit(-1);
  }

  // Trimite mesaj de confirmare
  send_message(&ACK);

  FILE *filePtr;
  char *fileName = (char *) malloc(100 * sizeof(char));
  sprintf(fileName, "%s.bk", r.payload);
  filePtr = fopen(fileName, "wb");

  while (1) {
    if (recv_message(&r) < 0){
      perror("Receive message");
      exit(-1);
    }

    // Trimite mesajul de confirmare
    send_message(&ACK);

    // se opreste la primirea mesajului de oprire
    if (r.len == 0) {
      break;
    }

    int wc = fwrite(r.payload, 1, r.len, filePtr);

    if (wc < 0) {
      perror("Write error");
      exit(-1);
    }
  }

  fclose(filePtr);

  return 0;
}
