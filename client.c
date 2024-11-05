#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define UDP_PORT 1204
#define BUFFER_SIZE 256
#define HELLO_MSG "HELLO"
#define CMD_MSG "CMD"
#define RESPONSE_MSG "RESPONSE"
#define STATUS_MSG "STATUS"
#define HEADER_SIZE 10
#define CMD_LENGTH 23
#define MAX_SERVERS 10

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
} Server;

Server servers[MAX_SERVERS];
int server_count = 0;

void add_server(const char *ip, int port) {
    for (int i = 0; i < server_count; i++) {
        if (strcmp(servers[i].ip, ip) == 0) return;  // Server already known
    }
    if (server_count < MAX_SERVERS) {
        strncpy(servers[server_count].ip, ip, INET_ADDRSTRLEN - 1);
        servers[server_count].ip[INET_ADDRSTRLEN - 1] = '\0';
        servers[server_count].port = port;
        server_count++;
        printf("Detected server: %s:%d\n", ip, port);
    }
}

void send_hello_to_server(int sockfd, struct sockaddr_in *server_addr) {
    char hello_msg[HEADER_SIZE];
    snprintf(hello_msg, HEADER_SIZE, "%s", HELLO_MSG);
    if (sendto(sockfd, hello_msg, strlen(hello_msg), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("Error sending HELLO");
    }
}

void check_servers(int sockfd) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    char buffer[BUFFER_SIZE];

    while (1) {
        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&addr, &addr_len);
        if (received_len < 0) {
            perror("Error receiving data");
            continue;
        }

        buffer[received_len] = '\0';
        char server_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, server_ip, INET_ADDRSTRLEN);
        printf("Received message from server IP: %s, Message: %s\n", server_ip, buffer);

        // Parse header and handle message
        char msg_type[HEADER_SIZE];
        sscanf(buffer, "%9s", msg_type);
        if (strcmp(msg_type, HELLO_MSG) == 0) {
            add_server(server_ip, ntohs(addr.sin_port));
            send_hello_to_server(sockfd, &addr);
        } else if (strcmp(msg_type, RESPONSE_MSG) == 0) {
            printf("Received response from server: %s\n", buffer);
        }
    }
}

int main() {
    srand(time(NULL));
    int sockfd;
    struct sockaddr_in client_addr;

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Bind to the specific client IP
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("192.168.56.108");  // Client's specific IP
    client_addr.sin_port = htons(UDP_PORT);
    if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Error binding");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Client started, waiting for HELLO messages...\n");

    if (fork() == 0) {
        check_servers(sockfd);
        exit(0);
    }

    // Okresowe wysyłanie poleceń do odkrytych serwerów
    while (1) {
        send_command_to_server(sockfd);
        usleep((1500 + rand() % 1001) * 1000);  // Losowe opóźnienie między 1500 a 2500 ms
    }

    close(sockfd);
    return 0;
}
