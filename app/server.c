#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main() {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	// You can use print statements as follows for debugging, they'll be visible when running tests.
	printf("Logs from your program will appear here!\n");

	// Uncomment this block to pass the first stage
	//
	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;
	
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
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
	
	int fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
	printf("Client connected\n");

	int len = 100;
	char reply[len];
	char response[len];
	// Receive response from client
	int bytes_recv = recv(fd, response, len, 0);

	int i = 0;
	// Find the firt / character
	while (response[i++] != '/' && i < len) ;
	int bytes_sent;
	if (response[i] == ' ') {
		// root
		strcpy(reply, "HTTP/1.1 200 OK\r\n\r\n");
	} else if (strncmp(&response[i], "echo", 4) == 0) {
		// echo end point
		// Find the second / character
		while (response[i++] != '/' && i < len) ;
		char echo[len];
		memset(echo, 0, len);
		int j = 0;
		while (response[i] != ' ' && i < len) {
			echo[j++] = response[i++];
		}
		sprintf(reply, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", strlen(echo), echo);
	} else if (strncmp(&response[i], "user-agent", 10) == 0) {
		// user-agent end point
		while (strncmp(&response[i++], "User-Agent", 10)) ;
		while (response[i++] != ' ') ;	// Traverse till user-agent value
		char agent[len];
		memset(agent, 0, len);
		int j = 0;
		while (response[i] != '\r' && j < len) {
			agent[j++] = response[i++];
		}
		sprintf(reply, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %ld\r\n\r\n%s", strlen(agent), agent);
	} else { // Not supported
		strcpy(reply, "HTTP/1.1 404 Not Found\r\n\r\n");
	}
	printf("---%s\n", reply);
	bytes_sent = send(fd, reply, strlen(reply), 0);

	close(server_fd);

	return 0;
}
