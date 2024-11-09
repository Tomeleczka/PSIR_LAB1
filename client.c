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
#define PONG_MSG "PONG"
#define PING_MSG "PING"
#define EXE_MSG "EXE"

#define HEADER_SIZE 10      // 10 znaków  na CMD_MSG, PONG_MSG, etc. + \n
#define CMD_LENGTH 30      // HEADER_SIZE + 25 znaków
#define EXE_LENGTH 32      // HEADER_SIZE + 28 znaków

// #define HEADER_SIZE 10
// #define CMD_LENGTH 26
// #define EXE_LENGTH 29

#define MAX_SERVERS 10

int sockfd;

struct timespec send_time;
struct timespec received_time;

// Struktura przechowująca informacje o serwerze
typedef struct{
    char ip[INET_ADDRSTRLEN];
    int port;
    char *id;
    bool flag;
    time_t last_ping_time;
} Server;

Server dictionary[MAX_SERVERS];
int server_count = 0;
static int last_id = 0;

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

// Funkcja generująca unikalne ID
const char* generate_unique_id(void) {
    static char id_buffer[32];
    
    last_id++;
    snprintf(id_buffer, sizeof(id_buffer), "ID%d", last_id);
    return id_buffer;
}

// Funkcja pomocnicza licząca ilość podłączonych serwerów w historii
int count_non_null_servers() {
    int count = 0;
    for (int i = 0; i < MAX_SERVERS; i++) {
        if (dictionary[i].id != NULL) {
            count++;
        }
    }
    return count;
}

// Funkcja zmieniająca flagę serwera
void change_flag(const char *id, bool new_flag)
{
    server_count = count_non_null_servers();
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

// Funkcja otrzymująca aktywne serwery
void get_active_ids(void) {
    printf("ID serwerów z aktywną flagą:\n");
    for (int i = 0; i < server_count; i++) {
        if (dictionary[i].flag) {
            printf("%s\n", dictionary[i].id);
        }
    }
}

// Funkcja dodająca serwer do listy, jeśli jeszcze nie jest na liście
void add_server(const char *ip, int port){
    // printf("Serwer się dodaje...\n");
    // Sprawdzamy, czy serwer już istnieje
    for (int i = 0; i < server_count; i++) {
        if (strcmp(dictionary[i].ip, ip) == 0 && dictionary[i].port == port) {
            change_flag(dictionary[i].id, true);
            dictionary[i].last_ping_time = time(NULL);
            return;
        }
    }
    if (server_count >= MAX_SERVERS) {
        printf("Error: Maksymalna ilość serwerów: (%d) jest osiągnięta. Nie można dodać więcej.\n", MAX_SERVERS);
        return;
    }
    
    Server *new_server = &dictionary[server_count];
    const char *new_id = generate_unique_id();
    new_server->id = strdup(new_id);
    strncpy(new_server->ip, ip, INET_ADDRSTRLEN);
    new_server->port = port;
    new_server->flag = true;
    new_server->last_ping_time = time(NULL); 

    server_count++;
    // printf("%d\n", server_count);
    printf("Dodano serwer o wartościach ID: %s, IP: %s, Port: %d, Flaga: %s\n",
           new_server->id, new_server->ip, new_server->port,
           new_server->flag ? "true" : "false");
    get_active_ids();
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
        char cmd_msg[CMD_LENGTH];
        snprintf(cmd_msg, sizeof(cmd_msg), "%s %s", CMD_MSG, generate_random_string());
        if (sendto(sockfd, cmd_msg, strlen(cmd_msg), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Błąd podczas wysyłania wiadomości CMD\n");
        } else {
            clock_gettime(CLOCK_REALTIME, &send_time);
            printf("Wysłano CMD do serwera IP: %s, Port: %d, Wiadomość: %s\n",
                   random_server.ip, random_server.port, cmd_msg);
        }
    } else {
        printf("Brak aktywnego serwera do wysłania CMD.\n");
    }
}

void update_server_timeouts(void) {
    time_t current_time = time(NULL);
    server_count = count_non_null_servers();
    // printf("%d\n", server_count);
    for (int i = 0; i < server_count; i++) {
        if (dictionary[i].flag && difftime(current_time, dictionary[i].last_ping_time) > 1) {
            // Mark server as inactive if it hasn’t responded within 410 ms
            change_flag(dictionary[i].id, false);
            printf("Serwer %s, IP: %s, Port: %d oznaczony jako nieaktywny (timeout).\n",
                   dictionary[i].id, dictionary[i].ip, dictionary[i].port);
        }
    }
}

// Funkcja wysyłająca wiadomość CMD_MSG do losowego aktywnego serwera
void send_ping_to_servers() {
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    // char message[] = "Ping";
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < server_count; i++) {
        if (dictionary[i].flag) {  

            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(dictionary[i].port);
            inet_pton(AF_INET, dictionary[i].ip, &server_addr.sin_addr);
                
            // Wysyłamy Ping
            if (sendto(sockfd, PING_MSG, strlen(PING_MSG), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
                perror("Błąd przy wysłaniu Ping");
                continue;
            }
        }
    }              
}

// Wątek CMD
void* cmd_sender_thread(void* arg) {
    while (1) {
        int min_time = 1500, max_time = 2150;
        int random_time = timeRandoms(min_time, max_time);
        usleep(random_time * 1000);  // Convert to microseconds
        send_cmd_to_random_server();

    }
    return NULL;
}

// Wątek PING :)
void* send_ping_thread(void* arg) {
    while (1) {
        usleep(410 * 1000);
        send_ping_to_servers();
    }
    return NULL;
}

// Wątek TIMEOUT
void* timeout_send_thread(void* arg) {
    while (1) {
        usleep(500 * 100);
        update_server_timeouts();
    }
    return NULL;
}

// Funkcja osługująca PONG
void pong_receive(const char *ip, int port){
    server_count = count_non_null_servers();
    // printf("Otrzymanie PONG...\n");
    for (int i = 0; i < server_count; i++) {
        if (dictionary[i].flag && strcmp(dictionary[i].ip, ip) == 0 && dictionary[i].port == port) {
            dictionary[i].last_ping_time = time(NULL);  // Update last ping time on PONG
            printf("Aktualizacja czasu ostatniego PONG dla serwera ID: %s\n", dictionary[i].id);
            return;
        }
    }
}

// Funkcja pomocniczna do wypisania treści 
void process_message(const char *exe_type, const char *exe_series) {
    if (strcmp(exe_type, "EXE") == 0) {
        clock_gettime(CLOCK_REALTIME, &received_time);
        
        time_t now;
        time(&now);
        char current_date[100];
        strftime(current_date, sizeof(current_date), "%Y-%m-%d %H:%M:%S", localtime(&now));

        // Czas od wysłania
        long delay_ns = (received_time.tv_sec - send_time.tv_sec) * 1000000000L + 
                (received_time.tv_nsec - send_time.tv_nsec);

        printf("Data: %s, Czas od wysłania (ns): %ld, Ciąg: %s\n",
        current_date, delay_ns, exe_series);
    }
}

int main(){
    srand(time(NULL));
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(server_addr);
    char buffer[BUFFER_SIZE];

    // Tworzenie gniazda UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0){
        perror("Błąd podczas tworzenia gniazda\n");
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

    printf("Klient uruchomiony.\n");

    // CMD_MSG
    pthread_t cmd_thread;
    if (pthread_create(&cmd_thread, NULL, cmd_sender_thread, NULL) != 0){
        perror("Błąd podczas tworzenia wątku CMD\n");
        exit(EXIT_FAILURE);
    }

    // PING_MSG
    pthread_t ping_thread;
    if (pthread_create(&ping_thread, NULL, send_ping_thread, NULL) != 0) {
        perror("Błąd podczas tworzenia wątku PING\n");
        exit(EXIT_FAILURE);
    }

    // TIMEOUT
    pthread_t timeout_thread;
    if (pthread_create(&timeout_thread, NULL, timeout_send_thread, NULL) != 0) {
        perror("Błąd podczas timeout\n");
        exit(EXIT_FAILURE);
    }

    while (1){
        // printf("Check\n");
        // Odbieranie wiadomości z gniazda
        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&server_addr, &addr_len);
        if (received_len < 0){
            perror("Błąd podczas odbierania danych\n");
            continue;
        }

        buffer[received_len] = '\0';
        char server_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &server_addr.sin_addr, server_ip, INET_ADDRSTRLEN);
        printf("Otrzymano wiadomość od serwera IP: %s, Wiadomość: %s\n", server_ip, buffer);

        // Parsowanie nagłówka i obsługa wiadomości
        char msg_type[HEADER_SIZE];
        char exe_type[HEADER_SIZE];
        char exe_series[EXE_LENGTH];
        sscanf(buffer, "%9s %25s", exe_type, exe_series);
        sscanf(buffer, "%9s", msg_type);

        if (strcmp(msg_type, HELLO_MSG) == 0) {
            add_server(server_ip, ntohs(server_addr.sin_port)); // Dodanie serwera
        } else if (strcmp(msg_type, PONG_MSG) == 0) {
            pong_receive(server_ip, ntohs(server_addr.sin_port));  // Aktualizacja czasu od ostatnio otrzymanego PONG
        } else if (strcmp(exe_type, EXE_MSG) == 0){
            process_message(exe_type, exe_series);
        }
    }

    // Czyszczenie
    pthread_cancel(cmd_thread);
    pthread_join(cmd_thread, NULL);
    pthread_cancel(timeout_thread);
    pthread_join(timeout_thread, NULL);
    pthread_cancel(ping_thread);
    pthread_join(ping_thread, NULL);

    // Zamknięcie gniazda po zakończeniu
    close(sockfd);
    return 0;
}

    
