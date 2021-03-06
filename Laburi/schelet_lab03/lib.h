#ifndef LIB
#define LIB

#define MSGSIZE		1400
#define COUNT		1000

typedef struct __attribute__((__packed__)) {
	char sum;
	char rest[1400 - sizeof(char)];
} mypkg;

typedef struct {
  int len;
  char payload[MSGSIZE];
} msg;

void init(char* remote,int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg* m);
int recv_message(msg* r);

#endif

