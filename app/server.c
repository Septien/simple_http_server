#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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

void removeq(queue_t *q, int i) {
	if (q->size == 0) return;
	q->arr[i] = 0;
	q->size--;
}

void process_GET(int fd, char *request, char *dir) {
	int i = 0;
	char response[LEN];
	memset(response, 0, LEN);
	while (request[i++] != '/' && i < LEN) ;
	// Handle endpoints
	if (request[i] == ' ') {
		// root
		strcpy(response, "HTTP/1.1 200 OK\r\n\r\n");
	} else if (strncmp(&request[i], "echo", 4) == 0) {
		// echo end point
		// Find the second / character
		while (request[i++] != '/' && i < LEN) ;
		char echo[LEN/10];
		memset(echo, 0, LEN/10);
		int j = 0;
		while (request[i] != ' ' && i < LEN/10) {
			echo[j++] = request[i++];
		}
		sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", strlen(echo), echo);
	} else if (strncmp(&request[i], "user-agent", 10) == 0) {
		// user-agent end point
		while (strncmp(&request[i++], "User-Agent", 10)) ;
		while (request[i++] != ' ') ;	// Traverse till user-agent value
		char agent[LEN/10];
		memset(agent, 0, LEN/10);
		int j = 0;
		while (request[i] != '\r' && j < LEN/10) {
			agent[j++] = request[i++];
		}
		sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", strlen(agent), agent);
	} else if (strncmp(&request[i], "files", 5) == 0) {
		// Send file content to client
		// Find the second / character
		while (request[i] != ' ' && request[i++] != '/' && i < LEN) ;
		char filename[LEN];
		memset(filename, 0, LEN);
		strcpy(filename, dir);
		int j = strlen(filename);
		while(request[i] != ' ') {
			filename[j++] = request[i++];
		}
		int f = open(filename, O_RDONLY);
		if (f == -1 || strlen(filename) == strlen(dir)) {
			strcpy(response, "HTTP/1.1 404 Not Found\r\n\r\n");
		} else {
			char content[LEN/2];
			memset(content, 0, LEN/2);
			int r = read(f, content, LEN/2);
			int err = errno;
			sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %ld\r\n\r\n%s", strlen(content), content);
		}
		close(f);

	} else { // Not supported
		strcpy(response, "HTTP/1.1 404 Not Found\r\n\r\n");
	}

	int bytes_sent = send(fd, response, strlen(response), 0);
	printf("Closing %d.\n", fd);
	close(fd);
}

void process_POST(int fd, char *request, char *dir) {
	int i = 0;
	char response[LEN];
	memset(response, 0, LEN);
	while (request[i++] != '/' && i < LEN) ;
	// Handle endpoints 
	if (strncmp(&request[i], "files", 5) == 0) {
		// Create new file and copy contents
		// Find the second / character
		while (request[i] != ' ' && request[i++] != '/' && i < LEN) ;
		char filename[LEN];
		memset(filename, 0, LEN);
		strcpy(filename, dir);
		int j = strlen(filename);
		while(request[i] != ' ') {
			filename[j++] = request[i++];
		}
		int f = open(filename, O_WRONLY | O_CREAT | S_IRWXU);
		if (f == -1 || strlen(filename) == strlen(dir)) {
			strcpy(response, "HTTP/1.1 404 Not Found\r\n\r\n");
		} else {
			while (strncmp(&request[i++], "Content-Length: ", 16) != 0) ;
			while (request[i++] != ' ') ;
			int n = 0;
			sscanf(&request[i], "%d", &n);
			int pos = strlen(request) - n;
			int r = write(f, &request[pos], n);
			int err = errno;
			strcpy(response, "HTTP/1.1 201 Created\r\n\r\n%s");
		}
		close(f);
	}

	int bytes_sent = send(fd, response, strlen(response), 0);
	printf("Closing %d.\n", fd);
	close(fd);
}

void process_request(int fd, char *dir) {
	// Clear memory
	// Receive request from client
	char request[LEN];
	int bytes_recv = recv(fd, request, LEN, 0);

	// Find the first / character
	printf("Processing request.\n");
	if (strncmp(request, "GET", 3) == 0) {
		process_GET(fd, request, dir);
	} else if (strncmp(request, "POST", 4) == 0) {
		process_POST(fd, request, dir);
	}
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

int main(int argc, char **argv) {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	char *dir = NULL;
	if (argc > 2) {
		dir = argv[2];
	}
	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	queue_t fds;		// Read sockets
	fd_set rset;
	init(&fds);
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
					FD_CLR(server_fd, &rset);
				}
				for (i = 0; i < CONNECTIONS; i++) {
					if (FD_ISSET(fds.arr[i], &rset)) {
						process_request(fds.arr[i], dir);
						FD_CLR(fds.arr[i], &rset);
						removeq(&fds, i);
					}
				}
				break;
		}
	}

	close(server_fd);
	return 0;
}
