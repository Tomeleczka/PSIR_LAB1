#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#define UDP_PORT 3605
#define BUFFER_SIZE 256
#define HELLO_MSG "HELLO"
#define CMD_MSG "CMD"

int main() {
    int server_id = getpid();  // Unikalne ID serwera na podstawie PID
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    
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

    // Powiązanie gniazda z określonym portem UDP
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Serwer %d uruchomiony!\n", server_id);

    // Przygotowanie adresu klienta, aby wysłać wiadomość
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("192.168.56.108");
    client_addr.sin_port = htons(UDP_PORT);

    // Wysłanie wiadomość powitalnej HELLO do klienta
     if (sendto(sockfd, HELLO_MSG, strlen(HELLO_MSG), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Error sending HELLO message");
    } else {
        printf("Sent HELLO message to client at 192.168.56.108\n");
    }

    while (1) {
        // Odbieranie dowolnej przychodzącej wiadomości
        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        
        // Obsłużenie błędy przy odbiorze wiadomości
        if (received_len < 0) {
            perror("Error receiving data");
            continue;
        }

        buffer[received_len] = '\0';
        printf("Otrzymanie wiadomości od klienta: %s\n", buffer);

        // Wypisanie IP klienta dla celów debugowania
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Otrzymano wiadomość od klienta IP: %s, Wiadomość: %s\n", client_ip, buffer);

        // // Parsowanie typu wiadomości
        // char msg_type[HEADER_SIZE];
        // sscanf(buffer, "%9s", msg_type);

        // // Sprawdzenie typu wiadomości i odpowiednie działanie
        // if (strcmp(msg_type, HELLO_MSG) == 0) {
        //     printf("Otrzymano wiadomość HELLO od klienta.\n");
        //     // Przygotowanie i wysłanie odpowiedzi HELLO do klienta
        //     char response_header[HEADER_SIZE];
        //     create_header(response_header, RESPONSE_MSG, server_id);
        //     sendto(sockfd, response_header, strlen(response_header), 0, (struct sockaddr *)&client_addr, addr_len);
        
        // } else if (strcmp(msg_type, CMD_MSG) == 0) {
        //     // Wyświetlenie otrzymanego polecenia
        //     printf("Otrzymano polecenie: %s\n", buffer + HEADER_SIZE);
        
        // } else if (strcmp(msg_type, STATUS_MSG) == 0) {
        //     // Wysłanie odpowiedzi STATUS do klienta
        //     printf("Otrzymano żądanie STATUS od klienta.\n");
        //     char status_response[HEADER_SIZE];
        //     create_header(status_response, STATUS_MSG, server_id);
        //     sendto(sockfd, status_response, strlen(status_response), 0, (struct sockaddr *)&client_addr, addr_len);
        // }
    }

    // Zamknięcie gniazda po zakończeniu pracy
    close(sockfd);
    return 0;
}
