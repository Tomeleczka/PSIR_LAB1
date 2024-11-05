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

// Funkcja do tworzenia nagłówka wiadomości
void create_header(char *header, const char *msg_type, int server_id) {
    snprintf(header, HEADER_SIZE, "%s_%04d", msg_type, server_id);
}

int main() {
    srand(time(NULL));
    int server_id = getpid();  // Unikalne ID serwera wykorzystujące PID
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    // Tworzenie gniazda UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Zerowanie struktury adresowej serwera
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDP_PORT);
    
    // Powiązanie gniazda z adresem i portem
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Serwer %d uruchomiony, oczekiwanie na wiadomości...\n", server_id);

    // Wysyłanie wiadomości HELLO okresowo
    char hello_message[HEADER_SIZE];
    create_header(hello_message, HELLO_MSG, server_id);
    
    while (1) {
        // Wysyłanie wiadomości HELLO do dowolnego klienta
        struct sockaddr_in broadcast_addr;
        memset(&broadcast_addr, 0, sizeof(broadcast_addr));
        broadcast_addr.sin_family = AF_INET;
        broadcast_addr.sin_addr.s_addr = inet_addr("192.168.56.225"); // Adres rozgłoszeniowy
        broadcast_addr.sin_port = htons(UDP_PORT);

        int broadcastEnable = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
            perror("Błąd ustawiania trybu broadcastu");
            exit(EXIT_FAILURE);
        }

        // Kod wysyłania wiadomości HELLO
        if (sendto(sockfd, hello_message, strlen(hello_message), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
            perror("Błąd podczas wysyłania wiadomości HELLO");
        } else {
            printf("Wysłano wiadomość HELLO\n");
        }
        sleep(2);  // Wysyłaj wiadomość HELLO co 2 sekundy
        // Odbieranie dowolnej przychodzącej wiadomości
        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        buffer[received_len] = '\0';

        // Parsowanie nagłówka
        char msg_type[HEADER_SIZE];
        sscanf(buffer, "%9s", msg_type);

        // Sprawdzenie typu wiadomości
        if (strcmp(msg_type, HELLO_MSG) == 0) {
            printf("Otrzymano wiadomość HELLO od klienta.\n");
            // Odpowiedz na wiadomość HELLO od klienta
            char response_header[HEADER_SIZE];
            create_header(response_header, RESPONSE_MSG, server_id);
            sendto(sockfd, response_header, strlen(response_header), 0, (struct sockaddr *)&client_addr, addr_len);
            continue;
        } else if (strcmp(msg_type, CMD_MSG) == 0) {
            printf("Otrzymano polecenie: %s\n", buffer + HEADER_SIZE);
            // Obsługa odpowiedzi na polecenie...
        } else if (strcmp(msg_type, STATUS_MSG) == 0) {
            printf("Otrzymano żądanie STATUS od klienta.\n");
            char status_response[HEADER_SIZE];
            create_header(status_response, STATUS_MSG, server_id);
            sendto(sockfd, status_response, strlen(status_response), 0, (struct sockaddr *)&client_addr, addr_len);
        }
    }

    close(sockfd);
    return 0;
}
