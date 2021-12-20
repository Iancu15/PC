#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	int fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror("deschidere");
		exit(-1);
	}

	char* letter;
    int file_counter = 0;
    int* v = malloc(10 * sizeof(int)); // positions
    v[0] = 0;
    int pos_counter = 1;
    int num_positions = 10;

    int rc = 1;
	while(rc != 0) {
		rc = read(fd, letter, sizeof(char));

		if (rc < 0) {
			perror("eroare la citire");
			exit(-1);
		}

        file_counter += rc;

        if (*letter == 10) {
            v[pos_counter] = file_counter;
            pos_counter++;

            if (pos_counter == num_positions) {
                num_positions *= 2;
                v = (int*) realloc(v, num_positions * sizeof(int));
            }
        }
	}

    char buffer[1024];
    // salvez sfarsitul de fisier
    v[pos_counter] = lseek(fd, 0, SEEK_CUR);
    int wc;
    for (int i = pos_counter - 1; i >= 0; i--) {
        lseek(fd, v[i], SEEK_SET);
        int rc = read(fd, buffer, sizeof(char) * (v[i + 1] - v[i]));

		if (rc < 0) {
			perror("eroare la citire");
			exit(-1);
		}

        wc = write(STDOUT_FILENO, buffer, rc);
		if (wc < 0) {
			perror("scriere");
		}

        // ultima linie are EOF in loc de newline la final, asa ca trebuie sa-l pun eu
        if (i == pos_counter - 1) {
            wc = write(STDOUT_FILENO, "\n", 1);
            if (wc < 0) {
                perror("scriere");
            }
        }
    }

    // pune newline-ul de la prima linie, nu mai trebuie sa pun eu
}