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
#define PONG_MSG "PONG"
#define PING_MSG "PING"
#define CMD_MSG "CMD"
#define EXE_MSG "EXE"

#define HEADER_SIZE 10      // 4 bajty na CMD_MSG, PONG_MSG, etc. + \n
#define CMD_LENGTH 30      // HEADER_SIZE + 25 znaków
#define EXE_LENGTH 32      // HEADER_SIZE + 28 znaków
#define HELLO_LENGTH 12    // HEADER_SIZE + PID (maskymalna dłguość 5 znaków)!!!

// #define HEADER_SIZE 10
// #define CMD_LENGTH 26
// #define EXE_LENGTH 29

int timeRandoms(int min, int max) {
    unsigned int seed = time(0);
    int rd_num = rand_r(&seed) % (max - min + 1) + min;
    return rd_num;
}

int sockfd;
struct sockaddr_in server_addr, client_addr;

void ping_receive(){
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    printf("Otrzymanie PING...\n");

    // Przygotowanie adresu klienta, aby wysłać wiadomość
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("192.168.56.108");
    client_addr.sin_port = htons(UDP_PORT);
                
    // Wysyłamy PONG
    if (sendto(sockfd, PONG_MSG, strlen(PONG_MSG), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0)
    {
        perror("Błąd wysyłania PONG\n");
    }
}

int main() {
    int server_id = getpid();  // Unikalne ID serwera na podstawie PID
    
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
        perror("Zły bind!\n");
        exit(EXIT_FAILURE);
    }

    printf("Serwer %d uruchomiony!\n", server_id);

    // Przygotowanie adresu klienta, aby wysłać wiadomość
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = inet_addr("192.168.56.108");
    client_addr.sin_port = htons(UDP_PORT);

    // Wysłanie wiadomość powitalnej HELLO do klienta
    char hello_msg[HELLO_LENGTH] = {0};
    snprintf(hello_msg, sizeof(hello_msg), "%s %d", HELLO_MSG, server_id);

    if (sendto(sockfd, hello_msg, strlen(hello_msg), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Błąd wysyłania HELLO\n");
    } else {
        printf("Wysłano wiadomość HELLO do klienta 192.168.56.108: %s\n", hello_msg);
    }

    while (1) {
        // Odbieranie dowolnej przychodzącej wiadomości
        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&client_addr, &addr_len);
        
        // Obsłużenie błędy przy odbiorze wiadomości
        if (received_len < 0) {
            perror("Błąd otrzymania danych");
            continue;
        }

        buffer[received_len] = '\0';
        // printf("Otrzymanie wiadomości od klienta: %s\n", buffer);

        // Wypisanie IP klienta dla celów debugowania
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Otrzymano wiadomość od klienta IP: %s, Wiadomość: %s\n", client_ip, buffer);


        struct timespec received_time;
        clock_gettime(CLOCK_REALTIME, &received_time);
        
        // Parsowanie nagłówka i obsługa wiadomości
        char msg_type[HEADER_SIZE] = {0};
        char cmd_type[HEADER_SIZE] = {0};
        char cmd_series[CMD_LENGTH] = {0};

        // Wyjęcie typu wiadomości
        sscanf(buffer, "%9s", msg_type);  // Reads the first word up to 9 characters

        if (strcmp(msg_type, PING_MSG) == 0) {
            ping_receive();  // Handle PING message
        } 
        else {
            // Kiedy na pewno nie PING sprawdzamy, czy COMMAND
            sscanf(buffer, "%9s %25s", cmd_type, cmd_series); 
            if (strcmp(cmd_type, CMD_MSG) == 0) {
                printf("Received CMD series: %s\n", cmd_series);

                int random_int = timeRandoms(100, 999);

                char exe_msg[EXE_LENGTH];
                snprintf(exe_msg, sizeof(exe_msg), "%s %d%s", EXE_MSG, random_int, cmd_series);

                if (sendto(sockfd, exe_msg, strlen(exe_msg), 0, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
                    perror("Błąd wysyłania wiadomości EXE");
                } else {
                    printf("Sent EXE message to client 192.168.56.108: %s\n", exe_msg);
                }
            }
        }
    }
    close(sockfd);
    return 0;
}
