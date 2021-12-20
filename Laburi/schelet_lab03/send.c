#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

char sum_check(char *payload, int len) {
	char ans = 0;
	for(int i = 0; i < len; i++) {
		ans ^= payload[i];
	}
	return ans;
}

int main(int argc, char *argv[])
{
	msg t, ack;
	mypkg p;
	int i, res;
	int BDP = atoi(argv[1]);
	int W = (BDP * 1000)/(1400*8);
	printf("Window size: %d\n", W);
	
	printf("[SENDER] Starting.\n");	
	init(HOST, PORT);

	/* printf("[SENDER]: BDP=%d\n", atoi(argv[1])); */
	if (W > COUNT) {
		W = COUNT;
	}

	/* create package and calculate checksum */
	memset(&t, 0, sizeof(msg));
	t.len = MSGSIZE;
	p.sum = sum_check(t.payload, t.len - sizeof(char));
	memcpy(p.rest, t.payload, MSGSIZE - sizeof(char));
	memcpy(t.payload, &p, sizeof(p));

	for (i = 0; i < W; i++) {		
		/* send msg */
		res = send_message(&t);
		if (res < 0) {
			perror("[SENDER] Send error. Exiting.\n");
			return -1;
		}
	}

	for (i = 0; i < COUNT-W; i++) {		
		/* wait for ACK */
		res = recv_message(&ack);
		if (res < 0) {
			perror("[SENDER] Receive error. Exiting.\n");
			return -1;
		}


		/* send msg */
		res = send_message(&t);
		if (res < 0) {
			perror("[SENDER] Send error. Exiting.\n");
			return -1;
		}
	}

	for (i = 0; i < W; i++) {
		/* wait for ACK */
		res = recv_message(&ack);
		if (res < 0) {
			perror("[SENDER] Receive error. Exiting.\n");
			return -1;
		}
	}

	printf("[SENDER] Job done, all sent.\n");
		
	return 0;
}
