#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>

#define UDP_PORT 1204
#define BUFFER_SIZE 256
#define HELLO_MSG "HELLO"
#define CMD_MSG "CMD"
#define MAX_SERVERS 10
#define HEADER_SIZE 10
#define INET_ADDRSTRLEN 16 


int sockfd;

// Funkcja, która obsługuje `Ctrl + C` (SIGINT) i `Ctrl + Z` (SIGTSTP) :D
void handle_exit(int signo) {
    printf("Closing client and releasing resources...\n");
    close(sockfd);
    printf("Exiting program...\n");
    exit(0);
}

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
const char *generate_unique_id(void) {
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

// Funkcja dodająca serwer do listy, jeśli jeszcze nie jest na liście
void add_server(const char *ip, int port)
{
    // Sprawdzamy, czy serwer juz istnieje
    for (int i = 0; i < server_count; i++)
    {
        if (strcmp(dictionary[i].ip, ip) == 0 && dictionary[i].port == port)
        {
            change_flag(dictionary[i].id, true);

            return;
        }
        if (server_count >= MAX_SERVERS)
        {
            printf("Error: Maksymalna liczba serwerów (%d) osiągnięta. Nie mozna dodac wiecej\n", MAX_SERVERS);
            return;
        }
        Server *new_server = &dictionary[server_count];
        const char *new_id = generate_unique_id();
        if (new_id == NULL) {
            printf("Błąd: Nie można wygenerować unikalnego ID dla serwera\n");
            return;
        }

        new_server->id = strdup(new_id);
        strncpy(new_server->ip, ip, INET_ADDRSTRLEN);
        new_server->ip[INET_ADDRSTRLEN - 1] = '\0';
        new_server->port = port;
        new_server->flag = true;

        server_count++;
        printf("Nowy serwer dodano do słownika:\n");
        printf("ID: %s, IP: %s, Port: %d, Flaga: %s\n",
               new_server->id, new_server->ip, new_server->port,
               new_server->flag ? "true" : "false");
    }
}



// Funkcja wysyłająca wiadomość HELLO do określonego serwera
// void send_hello_to_server(int sockfd, struct sockaddr_in *server_addr)
// {
//     char hello_msg[HEADER_SIZE];
//     snprintf(hello_msg, HEADER_SIZE, "%s", HELLO_MSG);
//     if (sendto(sockfd, hello_msg, strlen(hello_msg), 0, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
//     {
//         perror("Błąd podczas wysyłania wiadomości HELLO");
//     }
// }

// Funkcja sprawdzająca komunikaty od serwerów
void check_servers(int sockfd)
{
    printf("Czekam!");
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    char buffer[BUFFER_SIZE];

    while (1)
    {
        // Odbieranie wiadomości z gniazda
        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&addr, &addr_len);
        if (received_len < 0)
        {
            perror("Błąd podczas odbierania danych");
            continue;
        }

        buffer[received_len] = '\0';

        char server_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, server_ip, INET_ADDRSTRLEN);
        printf("Otrzymano wiadomość od serwera IP: %s, Wiadomość: %s\n", server_ip, buffer);

        // Parsowanie nagłówka i obsługa wiadomości
        char msg_type[HEADER_SIZE] = {0};
        sscanf(buffer, "%9s", msg_type);

        if (strcmp(msg_type, HELLO_MSG) == 0){
            printf("HELLO received\n");
            add_server(server_ip, ntohs(addr.sin_port));    // Dodajemy nowy serwer do listy
            // send_hello_to_server(sockfd, &addr);         // Wysyłamy wiadomość HELLO do serwera
        } else {
            printf("Received non-HELLO message: '%s'\n", msg_type);
        }
    }
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

int main(){

    if (signal(SIGINT, handle_exit) == SIG_ERR) {
        perror("Error setting SIGINT handler\n");
        return EXIT_FAILURE;
    }
    // if (signal(SIGTSTP, handle_exit) == SIG_ERR) {
    //     perror("Error setting SIGTSTP handler");
    //     return EXIT_FAILURE;
    // }

    srand(time(NULL));
    struct sockaddr_in client_addr;

    // Tworzenie gniazda UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Błąd podczas tworzenia gniazda");
        exit(EXIT_FAILURE);
    }

    // Set the SO_REUSEADDR option on the socket - zabij mnie, może to sprawia problemy, to ostatecznie killuje klienta
    int reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("Error setting SO_REUSEADDR");
        close(sockfd);
        return EXIT_FAILURE;
    }

    // ???
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(UDP_PORT);

    if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0){
        perror("Błąd podczas bindowania");
        close(sockfd);
        return EXIT_FAILURE;
    }

    printf("Klient uruchomiony, oczekiwanie na wiadomości HELLO...\n");

    // Uruchamiamy proces potomny do odbierania wiadomości od serwerów
    if (fork() == 0) {
        
        check_servers(sockfd);
        printf("Czy check_servers w ogóle działa?\n");
        close(sockfd);
        exit(0);
    }

    // Zamknięcie gniazda po zakończeniu
    close(sockfd);
    return 0;
}
