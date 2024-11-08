#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <sys/time.h>

#define UDP_PORT 3605
#define BUFFER_SIZE 256
#define HELLO_MSG "HELLO"
#define CMD_MSG "CMD"
#define HEADER_SIZE 10
#define CMD_LENGTH 23
#define MAX_SERVERS 10
#define PONG_MSG "PONG"

#define PING_INTERVAL_MS 410
#define PONG_TIMEOUT_MS 100

int sockfd;

// Funkcja zwracająca losową wartość z danego zakresu (od min do max)
// Potrzebna do losowego czasu wysłania wiadomości, poniżej przykład zastosowania (w środku main!!!)
//    int min = 1500, max = 2150;
//    int rd_num = timeRandoms(min, max)
int timeRandoms(int min, int max) {
    //printf("Random number between %d and %d: ", min, max);
    unsigned int seed = time(0);
    int rd_num = rand_r(&seed) % (max - min + 1) + min;
    //printf("%d ", rd_num);
    return rd_num;
}

//Funkcja generująca losowy 21-elementowy ciąg znaków
//Uwaga trzeba pamiętać że tablica jest statyczna
//Poniżej przykład użycia w main:
//srand((unsigned int) time(NULL));  
//printf("%s\n", generate_random_string());  
char *generate_random_string() {
    static char str[22];               
    char charset[] = "0123456789"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    size_t length = 21;
    for (size_t i = 0; i < length; i++) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        str[i] = charset[index];
    }
    str[length] = '\0';                
    return str;                        
}

// Struktura przechowująca informacje o serwerze
typedef struct
{
    char ip[INET_ADDRSTRLEN];
    int port;
    char *id;
    bool flag;
} Server;

Server dictionary[MAX_SERVERS];
int server_count = 0;
static int last_id = 0;

// Funkcja generująca unikalne ID
const char* generate_unique_id(void) {
    static char id_buffer[32];
    
    last_id++;
    snprintf(id_buffer, sizeof(id_buffer), "ID%d", last_id);
    return id_buffer;
}

// Funkcja zmieniająca flagę serwera
void change_flag(const char *id, bool new_flag)
{
    for (int i = 0; i < server_count; i++)
    {
        if (strcmp(dictionary[i].id, id) == 0)
        {
            dictionary[i].flag = new_flag;
            printf("Zmieniono flagę serwera o ID %s na %s\n", id, new_flag ? "true" : "false");
            return;
        }
    }
    printf("Nie znaleziono serwera o ID %s\n", id);
}

// Funkcja, otrzymująca aktywne serwery
void get_active_ids(void) {
    printf("ID z aktywną flagą:\n");
    for (int i = 0; i < server_count; i++) {
        if (dictionary[i].flag) {
            printf("%s\n", dictionary[i].id);
        }
    }
}

// Funkcja dodająca serwer do listy, jeśli jeszcze nie jest na liście
void add_server(const char *ip, int port)
{
    printf("Serwer się dodaje...\n");
    // Sprawdzamy, czy serwer już istnieje
    for (int i = 0; i < server_count; i++) {
        if (strcmp(dictionary[i].ip, ip) == 0 && dictionary[i].port == port) {
            change_flag(dictionary[i].id, true);
            return;
        }
    }
    if (server_count >= MAX_SERVERS) {
        printf("Error: Max servers (%d) reached. Cannot add more.\n", MAX_SERVERS);
        return;
    }
    
    Server *new_server = &dictionary[server_count];
    const char *new_id = generate_unique_id();
    new_server->id = strdup(new_id);
    strncpy(new_server->ip, ip, INET_ADDRSTRLEN);
    new_server->port = port;
    new_server->flag = true;

    server_count++;
    printf("%d\n", server_count);
    printf("Dodano serwer o wartościach ID: %s, IP: %s, Port: %d, Flaga: %s\n",
           new_server->id, new_server->ip, new_server->port,
           new_server->flag ? "true" : "false");
    get_active_ids();
}


// Funkcja wysyłająca wiadomość HELLO do określonego serwera
void send_hello_to_server(int sockfd, struct sockaddr_in *server_addr)
{
    char hello_msg[HEADER_SIZE];
    snprintf(hello_msg, HEADER_SIZE, "%s", HELLO_MSG);
    if (sendto(sockfd, hello_msg, strlen(hello_msg), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        perror("Błąd podczas wysyłania wiadomości HELLO");
    }
}

// Funkcja zwracająca losowy serwer z flagą ustawioną na true
// Przykład użycia w main:
// Server losowy_server = get_random_active_server();
Server get_random_active_server(void) {
    // Tablica do przechowywania indeksów serwerów z aktywną flagą
    int active_servers[MAX_SERVERS];
    int active_count = 0;
    // Zbieramy serwery, których flaga jest ustawiona na true
    for (int i = 0; i < server_count; i++) {
        if (dictionary[i].flag) {
            active_servers[active_count++] = i;
        }
    }
    // Jeśli nie ma aktywnych serwerów, zwróć pustą strukturę lub odpowiednią wartość
    if (active_count == 0) {
        printf("Brak aktywnych serwerów.\n");
        Server empty_server = {0};  // Tworzymy pusty serwer
        return empty_server;  // Zwracamy pustą strukturę
    }
    // Losujemy jeden serwer z aktywnych serwerów
    srand(time(NULL));  // Inicjalizowanie generatora liczb losowych
    int random_index = rand() % active_count;
    // Zwracamy losowy serwer i przy okazji printuje go 
    printf("Wylosowany aktywny serwer: %s, IP: %s, Port: %d\n", 
    dictionary[active_servers[random_index]].id, dictionary[active_servers[random_index]].ip, dictionary[active_servers[random_index]].port);
    return dictionary[active_servers[random_index]];
}

// Funkcja pomocnicza do printowania słownika
void print_dictionary(void) {
    printf("Serwery dodane do listy:\n");
    for (int i = 0; i < server_count; i++) {
        printf("ID: %s, IP: %s, Port: %d, Flaga: %s\n",
               dictionary[i].id,
               dictionary[i].ip,
               dictionary[i].port,
               dictionary[i].flag ? "true" : "false");
    }
}

// Funkcja wysyłająca wiadomość CMD_MSG do losowego aktywnego serwera
void send_cmd_to_random_server() {
    Server random_server = get_random_active_server();
    if (random_server.flag) { // Check if the server is valid (active)
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(random_server.port);
        inet_pton(AF_INET, random_server.ip, &server_addr.sin_addr);

        // Create CMD message
        char cmd_msg[HEADER_SIZE + CMD_LENGTH];
        snprintf(cmd_msg, sizeof(cmd_msg), "%s %s", CMD_MSG, generate_random_string());

        if (sendto(sockfd, cmd_msg, strlen(cmd_msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Błąd podczas wysyłania wiadomości CMD");
        } else {
            printf("Wysłano CMD do serwera IP: %s, Port: %d, Wiadomość: %s\n",
                   random_server.ip, random_server.port, cmd_msg);
        }
    } else {
        printf("Brak aktywnego serwera do wysłania CMD.\n");
    }
}

// Thread function to periodically send CMD messages
void* cmd_sender_thread(void* arg) {
    while (1) {
        int min_time = 1500, max_time = 2150;
        int random_time = timeRandoms(min_time, max_time);
        usleep(random_time * 1000);  // Convert to microseconds

        send_cmd_to_random_server();
    }
    return NULL;
}

// Funkcja wysyłająca wiadomość CMD_MSG do losowego aktywnego serwera
void send_ping_to_servers() {
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char message[] = "Ping";
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < server_count; i++) {
        if (dictionary[i].flag) {  

            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(dictionary[i].port);
            inet_pton(AF_INET, dictionary[i].ip, &server_addr.sin_addr);
                
            // Wysyłamy Ping
            if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
                perror("Błąd przy wysłaniu Ping");
                continue;
            }
        }
    }              
}

// Wątek dla ping :)
void* send_ping_thread(void* arg) {
    while (1) {
        usleep(410 * 1000);
        send_ping_to_servers();
    }
    return NULL;
}

void pong_receive(const char *ip, int port){

    printf("Otrzymanie PONG...\n");
    for (int i = 0; i < server_count; i++) {
        if (strcmp(dictionary[i].ip, ip) == 0 && dictionary[i].port == port) {
            change_flag(dictionary[i].id, false);
            printf("Zmieniono flagę serwera IP: %s, Port: %d na false\n", ip, port);
            return;
        }
    }
}

void check_for_pong_timeout(int sockfd) {
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 410000;  // 410 milliseconds

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);

    // Wait for data with a timeout
    int result = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);

    if (result == 0) {
        // Timeout occurred, no response within 410 ms
        printf("Brak odpowiedzi PONG, oznaczam serwery jako nieaktywne.\n");
        for (int i = 0; i < server_count; i++) {
            if (dictionary[i].flag) {
                change_flag(dictionary[i].id, false);
                printf("Serwer %s, IP: %s, Port: %d oznaczony jako nieaktywny\n",
                       dictionary[i].id, dictionary[i].ip, dictionary[i].port);
            }
        }
    } else if (result > 0 && FD_ISSET(sockfd, &read_fds)) {
        // There is data to read, call the function that handles receiving PONG
        struct sockaddr_in server_addr;
        socklen_t addr_len = sizeof(server_addr);
        char buffer[BUFFER_SIZE];

        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);
        if (received_len < 0) {
            perror("Błąd podczas odbierania danych");
            return;
        }

        buffer[received_len] = '\0';
        char msg_type[HEADER_SIZE];
        sscanf(buffer, "%9s", msg_type);

        if (strcmp(msg_type, PONG_MSG) == 0) {
            pong_receive(inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
        }
    } else {
        perror("Błąd podczas oczekiwania na dane");
    }
}


int main()
{
    srand(time(NULL));
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(server_addr);
    char buffer[BUFFER_SIZE];

    // Tworzenie gniazda UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Błąd podczas tworzenia gniazda");
        exit(EXIT_FAILURE);
    }

    // Zerowanie struktury adresowej klienta
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(UDP_PORT);

    // Powiązanie gniazda z określonym portem UDP
    if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    printf("Klient uruchomiony, oczekiwanie na wiadomości HELLO...\n");

    // CMD_MSG
    pthread_t cmd_thread;
    if (pthread_create(&cmd_thread, NULL, cmd_sender_thread, NULL) != 0) {
        perror("Błąd podczas tworzenia wątku CMD");
        exit(EXIT_FAILURE);
    }

    // PING_MSG
    pthread_t ping_thread;
    if (pthread_create(&ping_thread, NULL, send_ping_thread, NULL) != 0) {
        perror("Błąd podczas tworzenia wątku PING");
        exit(EXIT_FAILURE);
    }
    while (1)
    {

        // printf("Check\n");
        // Odbieranie wiadomości z gniazda
        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);
        if (received_len < 0)
        {
            perror("Błąd podczas odbierania danych");
            continue;
        }


        buffer[received_len] = '\0';
        char server_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &server_addr.sin_addr, server_ip, INET_ADDRSTRLEN);
        printf("Otrzymano wiadomość od serwera IP: %s, Wiadomość: %s\n", server_ip, buffer);

        // Parsowanie nagłówka i obsługa wiadomości
        char msg_type[HEADER_SIZE];
        sscanf(buffer, "%9s", msg_type);
        if (strcmp(msg_type, HELLO_MSG) == 0) {
            add_server(server_ip, ntohs(server_addr.sin_port)); // Add server on HELLO
            check_for_pong_timeout(sockfd);  // Check for PONG within 410 ms
        } else if (strcmp(msg_type, PONG_MSG) == 0) {
            pong_receive(server_ip, ntohs(server_addr.sin_port));  // Process PONG
        }
    }

    // Clean up
    pthread_cancel(cmd_thread);
    pthread_join(cmd_thread, NULL);
    pthread_cancel(ping_thread);
    pthread_join(ping_thread, NULL);
    // Zamknięcie gniazda po zakończeniu
    close(sockfd);
    return 0;
}

    
