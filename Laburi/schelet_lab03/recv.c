#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

char sum_check(char *payload, int len) {
	char ans = 0;
	for(int i = 0; i < len; i++) {
		ans ^= payload[i];
	}
	return ans;
}

int main(void)
{
	msg r;
	int i, res;
	
	printf("[RECEIVER] Starting.\n");
	init(HOST, PORT);

	memset(&r, 0, sizeof(msg));
	
	int corruptedPackages = 0;
	for (i = 0; i < COUNT; i++) {
		/* wait for message */
		res = recv_message(&r);
		if (res < 0) {
			perror("[RECEIVER] Receive error. Exiting.\n");
			return -1;
		}

		/* calculate checksum of received message */
		mypkg* pkgsteluta = (mypkg*)r.payload;
		int checksum = sum_check(pkgsteluta->rest, r.len - sizeof(char));
		
		/* test if the checksum is the same and increment accordingly */
		if (pkgsteluta->sum != checksum) {
			corruptedPackages++;
		}
		
		/* send dummy ACK */
		res = send_message(&r);
		if (res < 0) {
			perror("[RECEIVER] Send ACK error. Exiting.\n");
			return -1;
		}
	}

	printf("[RECEIVER] Finished receiving..\n");
	printf("Corrupted packages: %d\n", corruptedPackages);
	return 0;
}
