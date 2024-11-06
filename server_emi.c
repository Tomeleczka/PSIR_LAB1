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
#define COMMAND_MSG "CMD"
#define RESPONSE_MSG "RESPONSE"
#define RESPONSE_TO_COMMAND ""
#define HEADER_SIZE 10
#define CMD_LENGTH 23

// Funkcja do tworzenia nagłówka wiadomości
void create_header(char *header, const char *msg_type, int server_id) {
    snprintf(header, HEADER_SIZE, "%s_%04d", msg_type, server_id);
}

int main() {
    srand(time(NULL));
    int server_id = getpid();  // Unikalne ID serwera na podstawie PID
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

    // Powiązanie gniazda z określonym portem UDP
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Serwer %d uruchomiony, oczekiwanie na wiadomości...\n", server_id);

    // Przygotowanie wiadomości HELLO do wysyłania okresowo do konkretnego klienta
    char hello_message[HEADER_SIZE];
    create_header(hello_message, HELLO_MSG, server_id);
    
    while (1) {

        // Ustawienie adresu docelowego klienta
        struct sockaddr_in target_client_addr;
        memset(&target_client_addr, 0, sizeof(target_client_addr));
        target_client_addr.sin_family = AF_INET;
        target_client_addr.sin_addr.s_addr = inet_addr("192.168.56.108");
        target_client_addr.sin_port = htons(UDP_PORT);

        // // Wysłanie wiadomości HELLO bezpośrednio do klienta
        // if (sendto(sockfd, hello_message, strlen(hello_message), 0, (struct sockaddr *)&target_client_addr, sizeof(target_client_addr)) < 0) {
        //     perror("Błąd podczas wysyłania wiadomości HELLO");
        // } else {
        //     printf("Wysłano wiadomość HELLO do klienta na 192.168.56.108\n");
        // }
        // // sleep(2);  // Wysyłaj wiadomość HELLO co 2 sekundy

        // Odbieranie dowolnej przychodzącej wiadomości
        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        buffer[received_len] = '\0';

        // Wypisanie IP klienta dla celów debugowania
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Otrzymano wiadomość od klienta IP: %s, Wiadomość: %s\n", client_ip, buffer);

        // Parsowanie typu wiadomości
        char msg_type[HEADER_SIZE];
        sscanf(buffer, "%9s", msg_type);

        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&addr, &addr_len);
        if (received_len < 0) {
            perror("Błąd podczas odbierania danych");
            continue;
        }

        buffer[received_len] = '\0';
        char client_ip[INET_ADDRSTRLEN]; // Bufor na adres IP klienta
        inet_ntop(AF_INET, &addr.sin_addr, client_ip, INET_ADDRSTRLEN); // Konwersja adresu IP na czytelny format
        printf("Otrzymano wiadomość od serwera IP: %s, Wiadomość: %s\n", client_ip, buffer);


        // Sprawdzenie typu wiadomości i odpowiednie działanie
        // if (strcmp(msg_type, HELLO_MSG) == 0) {
        //     printf("Otrzymano wiadomość HELLO od klienta.\n");
        //     // Przygotowanie i wysłanie odpowiedzi HELLO do klienta
        //     char response_header[HEADER_SIZE];
        //     create_header(response_header, RESPONSE_MSG, server_id);
        //     sendto(sockfd, response_header, strlen(response_header), 0, (struct sockaddr *)&client_addr, addr_len);
        
        // } else 
        if (strcmp(msg_type, COMMAND_MSG) == 0){
            printf("Ciag: %s\n", buffer+HEADER_SIZE);
            char response_header[HEADER_SIZE];
            RESPONSE_TO_COMMAND = COMMAND_MSG + "1"
            create_header(response_header, RESPONSE_TO_COMMAND, server_id);
            COMMAND_MSG

        }
        // else if (strcmp(msg_type, CMD_MSG) == 0) {
        //     // Wyświetlenie otrzymanego polecenia
        //     printf("Otrzymano polecenie: %s\n", buffer + HEADER_SIZE);
        
        // } 
        // else if (strcmp(msg_type, STATUS_MSG) == 0) {
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
