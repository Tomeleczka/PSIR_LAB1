#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define UDP_PORT 1204
#define BUFFER_SIZE 256
#define HELLO_MSG "HELLO"
#define STATUS_MSG "STATUS"
#define CMD_MSG "CMD"
#define RESPONSE_MSG "RESPONSE"
#define HEADER_SIZE 10
#define CMD_LENGTH 23

// Function to create a message header
void create_header(char *header, const char *msg_type, int server_id) {
    snprintf(header, HEADER_SIZE, "%s_%04d", msg_type, server_id);
}

int main() {
    srand(time(NULL));
    int server_id = getpid();  // Unique server ID using PID
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Zero out the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_PORT);

    // Bind the socket to the specified port
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Server %d started, waiting for messages...\n", server_id);

    // Sending HELLO message periodically to the client's specific IP
    char hello_message[HEADER_SIZE];
    create_header(hello_message, HELLO_MSG, server_id);
    
    // Target client IP instead of broadcast
    struct sockaddr_in target_client_addr;
    memset(&target_client_addr, 0, sizeof(target_client_addr));
    target_client_addr.sin_family = AF_INET;
    target_client_addr.sin_addr.s_addr = inet_addr("192.168.56.108");  // Specific client IP
    target_client_addr.sin_port = htons(UDP_PORT);

    while (1) {
        // Send HELLO message directly to the client
        if (sendto(sockfd, hello_message, strlen(hello_message), 0, (struct sockaddr *)&target_client_addr, sizeof(target_client_addr)) < 0) {
            perror("Error sending HELLO message");
        } else {
            printf("HELLO message sent to client at 192.168.56.108\n");
        }
        sleep(2);  // Send HELLO every 2 seconds

        // Receive any incoming message
        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        buffer[received_len] = '\0';

        // Extract client IP for debugging
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Received message from client IP: %s, Message: %s\n", client_ip, buffer);

        // Parse message type
        char msg_type[HEADER_SIZE];
        sscanf(buffer, "%9s", msg_type);

        // Check message type and respond accordingly
        if (strcmp(msg_type, HELLO_MSG) == 0) {
            printf("Received HELLO message from client.\n");
            char response_header[HEADER_SIZE];
            create_header(response_header, RESPONSE_MSG, server_id);
            sendto(sockfd, response_header, strlen(response_header), 0, (struct sockaddr *)&client_addr, addr_len);
        } else if (strcmp(msg_type, CMD_MSG) == 0) {
            printf("Received command: %s\n", buffer + HEADER_SIZE);
            // Additional command handling...
        } else if (strcmp(msg_type, STATUS_MSG) == 0) {
            printf("Received STATUS request from client.\n");
            char status_response[HEADER_SIZE];
            create_header(status_response, STATUS_MSG, server_id);
            sendto(sockfd, status_response, strlen(status_response), 0, (struct sockaddr *)&client_addr, addr_len);
        }
    }

    close(sockfd);
    return 0;
}
