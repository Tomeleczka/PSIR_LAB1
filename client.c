#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>

#define UDP_PORT 3605
#define BUFFER_SIZE 256
#define HELLO_MSG "HELLO"
#define CMD_MSG "CMD"
#define MAX_SERVERS 10
#define HEADER_SIZE 10
#define INET_ADDRSTRLEN 16 

#define PING_INTERVAL_MS 410
#define PONG_TIMEOUT_MS 100

// Definicja MUTEX ;)
pthread_mutex_t dictionary_mutex = PTHREAD_MUTEX_INITIALIZER;


int sockfd;
int pipefd[2];

// Funkcja, która obsługuje `Ctrl + C` (SIGINT) i `Ctrl + Z` (SIGTSTP) :D
void handle_exit(int signo) {
    printf("Closing client and releasing resources...\n");
    close(sockfd);
    close(pipefd[0]);
    close(pipefd[1]);
    pthread_mutex_destroy(&dictionary_mutex);
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
    pthread_mutex_lock(&dictionary_mutex);
    printf("Serwer się dodaje...\n");
    // Sprawdzamy, czy serwer już istnieje
    for (int i = 0; i < server_count; i++) {
        if (strcmp(dictionary[i].ip, ip) == 0 && dictionary[i].port == port) {
            change_flag(dictionary[i].id, true);
            pthread_mutex_unlock(&dictionary_mutex);
            return;
        }
    }
    if (server_count >= MAX_SERVERS) {
        printf("Error: Max servers (%d) reached. Cannot add more.\n", MAX_SERVERS);
        pthread_mutex_unlock(&dictionary_mutex);
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

    pthread_mutex_unlock(&dictionary_mutex);
}

// Przetwarzanie komunikatów i dodawanie serwerów, itd.
void worker_process() {
    close(pipefd[1]); // Zamknięcie procesu check_servers

    char ip[INET_ADDRSTRLEN];
    int port;
    
    while (1) {
        // Read from the pipe
        if (read(pipefd[0], ip, INET_ADDRSTRLEN) > 0 && read(pipefd[0], &port, sizeof(int)) > 0) {
            add_server(ip, port);
        }
    }
}

struct timespec sent_times[MAX_SERVERS];
// Funkcja sprawdzająca komunikaty od serwerów
void check_servers(){
    // printf("Czekam!");
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    char buffer[BUFFER_SIZE];

    close(pipefd[0]);

    while (1)
    {
        // Odbieranie wiadomości z gniazda
        int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, 
        (struct sockaddr *)&addr, &addr_len);
        if (received_len < 0)
        {
            perror("Błąd podczas odbierania danych");
            continue;
        }

        buffer[received_len] = '\0';

        char server_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr, server_ip, INET_ADDRSTRLEN);
        printf("Otrzymano wiadomość od serwera IP: %s, Wiadomość: %s\n", server_ip, buffer);

        struct timespec received_time;
        clock_gettime(CLOCK_REALTIME, &received_time);

        // Parsowanie nagłówka i obsługa wiadomości
        char msg_type[HEADER_SIZE] = {0};
        sscanf(buffer, "%9s", msg_type);

        if (strcmp(msg_type, HELLO_MSG) == 0){
            printf("HELLO received\n");
            // Write server info to the pipe for the worker process
            write(pipefd[1], server_ip, INET_ADDRSTRLEN);
            int port = ntohs(addr.sin_port);
            write(pipefd[1], &port, sizeof(int));
        } else if (strcmp(msg_type, CMD_MSG) == 0) {
            // odpowiedz na komende od serwera
            int server_index = -1;
            for (int i = 0; i < server_count; i++) {
                if (strcmp(dictionary[i].ip, server_ip) == 0 && dictionary[i].port == ntohs(addr.sin_port)) {
                    server_index = i;
                    break;
                }
            }
            

            // liczymy delay po wyslaniu komendy do serwera
            long ms_delay = (received_time.tv_sec - sent_times[server_index].tv_sec) * 1000 +
                            (received_time.tv_nsec - sent_times[server_index].tv_nsec) / 1000000;

            char time_str[100];
            struct tm *tm_info = localtime(&received_time.tv_sec);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

            
            printf("[%s] Otrzymano wiasomosc od serwera %s:%d po czasie %ld ms: %s\n",
                   time_str, server_ip, ntohs(addr.sin_port), ms_delay, buffer + HEADER_SIZE);
        } 
        // else {
        //     printf("Received non-HELLO message: '%s'\n", msg_type);
        // }
    }
}

// Funkcja, otrzymująca aktywne serwery
void get_active_ids(void) {
    pthread_mutex_lock(&dictionary_mutex);
    printf("ID z aktywną flagą:\n");
    for (int i = 0; i < server_count; i++) {
        if (dictionary[i].flag) {
            printf("%s\n", dictionary[i].id);
        }
    }
    pthread_mutex_unlock(&dictionary_mutex);
}

// Funkcja zwracająca losowy serwer z flagą ustawioną na true
// Przykład użycia w main:
// Server losowy_server = get_random_active_server();
Server get_random_active_server(void) {
    // Tablica do przechowywania indeksów serwerów z aktywną flagą
    int active_servers[MAX_SERVERS];
    int active_count_p = 0;
    printf("%d\n", server_count);
    // Zbieramy serwery, których flaga jest ustawiona na true
    for (int i = 0; i < server_count; i++) {
        if (dictionary[i].flag) {
            printf("Check#1\n");
            active_count_p++;
            active_servers[active_count_p] = i;
        }
        printf("Check#2\n");
    }
    // Jeśli nie ma aktywnych serwerów, zwróć pustą strukturę lub odpowiednią wartość
    if (active_count_p == 0) {
        printf("Brak aktywnych serwerów.\n");
        Server empty_server = {0};  // Tworzymy pusty serwer
        return empty_server;  // Zwracamy pustą strukturę
    }
    // Losujemy jeden serwer z aktywnych serwerów
    srand(time(NULL));  // Inicjalizowanie generatora liczb losowych
    int random_index = rand() % active_count_p;

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

// Funkcja wysyłająca losową wiadomość do losowo wybranego aktywnego serwera
void *send_random_command(void *arg) {
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    while (1) {
        // Ustawienie losowego czasu opóźnienia
        int delay_ms = timeRandoms(1500, 2500);
        usleep(delay_ms * 1000);

        // Blokowanie mutexu, aby uzyskać dostęp do listy serwerów
        pthread_mutex_lock(&dictionary_mutex);

        // printf("%d\n", server_count);
        // Sprawdzenie, czy są jakieś aktywne serwery
        if (server_count == 0) {
            printf("Waiting for active servers...\n");
        }
        pthread_mutex_unlock(&dictionary_mutex);

        // Pobranie losowego aktywnego serwera
        Server random_server = get_random_active_server();

        // Odblokowanie mutexu po zakończeniu operacji na liście serwerów
        pthread_mutex_unlock(&dictionary_mutex);

        // Sprawdzenie, czy serwer jest poprawnie wylosowany
        if (random_server.id == NULL || strlen(random_server.ip) == 0) {
            printf("Nie można wysłać wiadomości: brak wylosowanego serwera.\n");
            continue;
        }

        // Konfiguracja adresu serwera
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(random_server.port);
        inet_pton(AF_INET, random_server.ip, &server_addr.sin_addr);

        // Generowanie losowej wiadomości o długości 23 znaków
        char message[24];
        snprintf(message, sizeof(message), "%s %s", CMD_MSG, generate_random_string());
        // fix czasu wyslania
        for (int i = 0; i < server_count; i++) {
            if (strcmp(dictionary[i].id, random_server.id) == 0) {
                clock_gettime(CLOCK_REALTIME, &sent_times[i]);
                break;
            }
        }
        // Wysłanie wiadomości do serwera
        if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
            perror("Błąd podczas wysyłania wiadomości CMD");
        } else {
            printf("Wysłano wiadomość CMD: '%s' do serwera %s:%d\n", message, random_server.ip, random_server.port);
        }
    }
    return NULL;
}

void *ping_servers(void *arg) {
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    char message[] = "Ping";
    char buffer[BUFFER_SIZE];
    struct timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = PONG_TIMEOUT_MS * 1000000L; // konwertujemy do ns

    while (1) {
        pthread_mutex_lock(&dictionary_mutex);
        
        for (int i = 0; i < server_count; i++) {
            if (dictionary[i].flag) {  
                
                memset(&server_addr, 0, sizeof(server_addr));
                server_addr.sin_family = AF_INET;
                server_addr.sin_port = htons(dictionary[i].port);
                inet_pton(AF_INET, dictionary[i].ip, &server_addr.sin_addr);

                // Wysylamy Ping
                if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&server_addr, addr_len) < 0) {
                    perror("Błąd przy wysłaniu Ping");
                    continue;
                }
                
                // Czekamy przed dostaniem Pong
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(sockfd, &fds);

                int retval = pselect(sockfd + 1, &fds, NULL, NULL, &timeout, NULL);
                if (retval == -1) {
                    perror("Błąd podczas oczekiwania wiadomości Pong");
                    continue;
                } else if (retval > 0 && FD_ISSET(sockfd, &fds)) {
                    // Otrzymujemy odpowiedz
                    int received_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);
                    if (received_len > 0) {
                        buffer[received_len] = '\0';
                        if (strcmp(buffer, "Pong") == 0) {
                            printf("Serwer %s jest aktywny (ID: %s)\n", dictionary[i].ip, dictionary[i].id);
                            continue;
                        }
                    }
                }

                // Jezeli ni ma odpowiedzi, zmieniamy flage
                printf("Serwer %s (ID: %s) nie odpowiada. Zmieniam flagę.\n", dictionary[i].ip, dictionary[i].id);
                
                change_flag(dictionary[i].id, false);
            }
        }
        
        pthread_mutex_unlock(&dictionary_mutex);
        
        // Сzekamy 410ms przed następnym pingowaniem
        usleep(PING_INTERVAL_MS * 1000); // do ms
    }

    return NULL;
}
int main(){

    

    if (signal(SIGINT, handle_exit) == SIG_ERR) {
        perror("Error setting SIGINT handler\n");
        return EXIT_FAILURE;
    }

    srand(time(NULL));
    struct sockaddr_in client_addr;

    // Tworzenie gniazda UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Błąd podczas tworzenia gniazda");
        exit(EXIT_FAILURE);
    }

    // Ustawienie opcji SO_REUSEADDR na gnieździe - zabijcie mnie, może to sprawia problemy, to ostatecznie killuje klienta
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

    printf("Klient uruchomiony\n");

    // Create a pipe for IPC !!! NOWEEEEEEEEEEEEEEEEEEEEEEEEEEE
    if (pipe(pipefd) == -1) {
        perror("Error creating pipe");
        close(sockfd);
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("Error forking");
        close(sockfd);
        return EXIT_FAILURE;
    } else if (pid == 0) {
        // Child process - worker
        worker_process();
    } else {
        pthread_t send_cmd_thread, ping_thread;
        if (pthread_create(&send_cmd_thread, NULL, send_random_command, NULL) != 0) {
            perror("Błąd przy tworzeniu wątku dla send_random_command");
            close(sockfd);
            return EXIT_FAILURE;
        }

        if (pthread_create(&ping_thread, NULL, ping_servers, NULL) != 0) {
            perror("Błąd przy tworzeniu wątku dla ping_servers");
            close(sockfd);
            return EXIT_FAILURE;
        }
        check_servers();
        pthread_join(send_cmd_thread, NULL);
        pthread_join(ping_thread, NULL);
    }
}
    // Chyba jakoś tak by to wyglądało(?), jeszcze dodanie losowego ciągu
    // while (1) {
    //     get_active_ids();  // Wypisujemy dostępne serwery
    //     Server random_server = get_random_active_server();
    //     if (random_server.id[0] != 0) {
    //         // Losowy serwer znaleziony, możemy wysłać wiadomość
    //         printf("Wysyłam CMD do serwera %s, IP: %s\n", random_server.id, random_server.ip);
    //         sendto(sockfd, CMD_MSG, strlen(CMD_MSG), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
    //     }
    //     sleep(timeRandoms(5, 10));  // Czekamy losowy czas przed kolejną próbą
    // }

    // !!!Można wykorzystać do wysyłania losowego ciągu!!!
    // !!!
    // char cmd_message[BUFFER_SIZE];
    // snprintf(cmd_message, BUFFER_SIZE, "%s Hej_Kociaki", CMD_MSG);

    // if (sendto(sockfd, cmd_message, strlen(cmd_message), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    //     perror("Error sending CMD message");
    // } else {
    //     printf("Sent CMD message: '%s' to server %s:%d\n", cmd_message, ip, port);
    // }
    // !!!    

