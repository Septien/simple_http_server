#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>

#define CONNECTIONS 300
#define LEN 1000

typedef struct queue {
	int arr[CONNECTIONS];
	int size;
} queue_t;

typedef struct data {
	char replies[CONNECTIONS][LEN];
} data_t;

void init_data(data_t *data) {
	memset(data, 0, sizeof(data));
}

void init(queue_t *q) {
	memset(q, 0, sizeof(queue_t));
}

void push(queue_t *q, int fd) {
	if (q->size > CONNECTIONS) return;
	for (int i = 0; i < CONNECTIONS; i++) {
		if (q->arr[i] == 0) {
			q->arr[i] = fd;
			break;
		}
	}
	q->size++;
}

void removeq(queue_t *q, int fd, int i) {
	if (q->size == 0) return;
	q->arr[i] = 0;
	q->size--;
}

void process_read(int fd, data_t *replies, int idx) {
	// Receive response from client
	char response[LEN];
	int bytes_recv = recv(fd, response, LEN, 0);

	int i = 0;
	// Find the first / character
	printf("Processing read.\n");
	while (response[i++] != '/' && i < LEN) ;
	// Clear memory
	memset(replies->replies[idx], 0, sizeof(replies->replies[idx]));
	if (response[i] == ' ') {
		// root
		strcpy(replies->replies[idx], "HTTP/1.1 200 OK\r\n\r\n");
	} else if (strncmp(&response[i], "echo", 4) == 0) {
		// echo end point
		// Find the second / character
		while (response[i++] != '/' && i < LEN) ;
		char echo[LEN];
		memset(echo, 0, LEN);
		int j = 0;
		while (response[i] != ' ' && i < LEN) {
			echo[j++] = response[i++];
		}
		sprintf(replies->replies[idx], "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", strlen(echo), echo);
	} else if (strncmp(&response[i], "user-agent", 10) == 0) {
		// user-agent end point
		while (strncmp(&response[i++], "User-Agent", 10)) ;
		while (response[i++] != ' ') ;	// Traverse till user-agent value
		char agent[LEN];
		memset(agent, 0, LEN);
		int j = 0;
		while (response[i] != '\r' && j < LEN) {
			agent[j++] = response[i++];
		}
		sprintf(replies->replies[idx], "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", strlen(agent), agent);
	} else { // Not supported
		strcpy(replies->replies[idx], "HTTP/1.1 404 Not Found\r\n\r\n");
	}

	int bytes_sent = send(fd, replies->replies[idx], strlen(replies->replies[idx]), 0);
	printf("Closing %d.\n", fd);
	close(fd);
}

void handle_error(int err) {
	switch(err) {
		case EBADF:
			printf("Invalid file descriptor.\n");
			break;
		case EINTR:
			printf("A signal was caught.\n");
			break;
		case EINVAL:
			printf("nfds is invalid or exceedes the RLIMIT_NOFILE resource limit\n");
			break;
		case ENOMEM:
			printf("Unable to allocate memory.\n");
			break;
		case EAGAIN:
			printf("Try again.\n");
			break;
		default:
			printf("Unrecognized error: %d.\n", err);
			break;
	}
}

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	data_t replies;		// Store the replies
	queue_t fds;		// Read sockets
	fd_set rset;
	init(&fds);
	init_data(&replies);
	FD_ZERO(&rset);

	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// Since the tester restarts your program quite often, setting SO_REUSEADDR
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(4221),
									 .sin_addr = { htonl(INADDR_ANY) },
									};
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	if (listen(server_fd, CONNECTIONS) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	
	printf("Socket server: %d\n", server_fd);
	printf("Waiting for clients to connect...\n");
	client_addr_len = sizeof(client_addr);

	while (1) {
		// Fill reading and writing set
		int i;
		FD_ZERO(&rset);
		FD_SET(server_fd, &rset);
		int fdmax = server_fd;
		int count = 0;
		for (i = 0; i < CONNECTIONS; i++) {
			if (fds.arr[i] > 0) {
				FD_SET(fds.arr[i], &rset);
				fdmax = (fds.arr[i] > fdmax ? fds.arr[i] : fdmax);
			}
		}
		printf("%d\n", count);
		int ret = select(fdmax + 1, &rset, NULL, NULL, NULL);
		int err = errno;
		switch(ret) {
			case -1:
				if (err == EINTR) continue;
				handle_error(err);
				close(server_fd);
				return 0;
			default:
				if (FD_ISSET(server_fd, &rset)) {
					int fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
					if (fd != -1) {
						printf("New client connected %d.\n", fd);
						push(&fds, fd);
					}
					printf("Connected clients: %d.\n", fds.size);
					FD_CLR(server_fd, &rset);
				}
				for (i = 0; i < CONNECTIONS; i++) {
					if (FD_ISSET(fds.arr[i], &rset)) {
						process_read(fds.arr[i], &replies, i);
						FD_CLR(fds.arr[i], &rset);
						removeq(&fds, fds.arr[i], i);
					}
				}
				break;
		}
	}

	close(server_fd);
	return 0;
}
