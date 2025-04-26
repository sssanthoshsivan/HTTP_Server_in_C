#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>

// Function to check if a string starts with a prefix
bool starts_with(const char *pre, const char *str)
{
	size_t lenpre = strlen(pre), lenstr = strlen(str);
	return lenstr < lenpre ? false : memcmp(pre, str, lenpre) == 0;
}

void *handle_client(void *arg)
{
	int client_fd = *((int *)arg);
	free(arg);

	char readBuffer[4096] = {0};

	while (1)
	{
		int bytes_read = read(client_fd, readBuffer, sizeof(readBuffer) - 1);
		if (bytes_read < 0)
		{
			printf("Read failed: %s\n", strerror(errno));
			break;
		}
		if (bytes_read == 0)
		{
			printf("Client disconnected\n");
			break;
		}

		printf("Request received:\n%.*s\n", bytes_read, readBuffer);

		// Parse request
		char *method = strtok(readBuffer, " ");
		char *path = strtok(NULL, " ");

		if (method == NULL || path == NULL)
		{
			printf("Invalid request\n");
			break;
		}

		int bytes_sent = 0;

		if (strcmp(path, "/") == 0)
		{
			char *reply = "HTTP/1.1 200 OK\r\n\r\n";
			bytes_sent = send(client_fd, reply, strlen(reply), 0);
		}
		else if (starts_with("/echo/", path))
		{
			char response[1024];
			char *echo_text = path + strlen("/echo/");
			snprintf(response, sizeof(response),
					 "HTTP/1.1 200 OK\r\n"
					 "Content-Type: text/plain\r\n"
					 "Content-Length: %ld\r\n"
					 "\r\n"
					 "%s",
					 strlen(echo_text), echo_text);
			bytes_sent = send(client_fd, response, strlen(response), 0);
		}
		else if (strncmp(path, "/user-agent", 11) == 0)
		{
			strtok(0, "\r\n");
			strtok(0, "\r\n");
			char *userAgent = strtok(0, "\r\n") + 12;
			const char *format = "HTTP/1.1 200 OK\r\nContent-Type: "
								 "text/plain\r\nContent-Length: %zu\r\n\r\n%s";
			char response[1024];
			sprintf(response, format, strlen(userAgent), userAgent);
			send(client_fd, response, sizeof(response), 0);
		}
		else
		{
			char *reply = "HTTP/1.1 404 Not Found\r\n\r\n";
			bytes_sent = send(client_fd, reply, strlen(reply), 0);
		}

		if (bytes_sent < 0)
		{
			printf("Send failed: %s\n", strerror(errno));
			break;
		}

		memset(readBuffer, 0, sizeof(readBuffer)); // clear buffer for next read
	}

	close(client_fd);
	return NULL;
}

int main()
{
	// Disable output buffering
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	int server_fd, client_addr_len;
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1)
	{
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
	{
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET,
		.sin_port = htons(4221),
		.sin_addr = {htonl(INADDR_ANY)},
	};

	if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	if (listen(server_fd, SOMAXCONN) != 0)
	{
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Server listening on port 4221...\n");

	while (1)
	{
		client_addr_len = sizeof(client_addr);
		int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
		if (client_fd < 0)
		{
			printf("Accept failed\n");
			continue;
		}
		printf("Client connected\n");

		pthread_t thread_id;
		int *pclient = malloc(sizeof(int));
		*pclient = client_fd;
		if (pthread_create(&thread_id, NULL, handle_client, pclient) != 0)
		{
			printf("Failed to create thread: %s\n", strerror(errno));
			close(client_fd);
			free(pclient);
			continue;
		}
		pthread_detach(thread_id);

		printf("Thread Created\n");
	}

	close(server_fd);
	return 0;
}
