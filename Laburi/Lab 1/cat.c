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

	char buff[1024];
	int wc;
	while(1) {
		int rc = read(fd, buff, sizeof(buff));

		if(rc == 0) {
			break;
		}

		if (rc < 0) {
			perror("eroare la citire");
			exit(-1);
		}

		wc = write(STDOUT_FILENO, buff, rc);
		if (wc < 0) {
			perror("scriere");
		}
	}

	wc = write(STDOUT_FILENO, "\n", 1);
    if (wc < 0) {
		perror("scriere");
	}
}
