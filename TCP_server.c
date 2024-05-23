#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Inclusie van headers afhankelijk van het besturingssysteem
#ifdef _WIN32
    #define _WIN32_WINNT _WIN32_WINNT_WIN7
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <stdint.h>
    #include <windows.h>

    // Functie om Windows sockets te initialiseren
    void initializeWindowsSockets() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0) {
            fprintf(stderr, "WSAStartup failed with error: %d\n", WSAGetLastError());
            exit(EXIT_FAILURE);
        }
    }

    // Functie om Windows sockets op te schonen
    void cleanupWindowsSockets() {
        WSACleanup();
    }

    // Macro om fouten te rapporteren met betrekking tot Winsock
    #define reportError(msg) fprintf(stderr, msg ": WSA-fout = %d\n", WSAGetLastError())
    // Macro voor het sluiten van een socket
    #define CLOSESOCKET closesocket
    // Macro's voor het afsluiten van lezen, schrijven en beide voor een socket
    #define SHUT_RD SD_RECEIVE
    #define SHUT_WR SD_SEND
    #define SHUT_RDWR SD_BOTH

    // Functie om een IP-adres in tekstuele notatie om te zetten naar een binair formaat
    int inet_pton(int af, const char *src, void *dst) {
        struct sockaddr_storage ss;
        int size = sizeof(ss);
        char src_copy[INET6_ADDRSTRLEN+1];

        ZeroMemory(&ss, sizeof(ss));
        strncpy(src_copy, src, INET6_ADDRSTRLEN+1);
        src_copy[INET6_ADDRSTRLEN] = 0;

        if (WSAStringToAddress(src_copy, af, NULL, (struct sockaddr *)&ss, &size) == 0) {
            switch(af) {
                case AF_INET:
                    *(struct in_addr *)dst = ((struct sockaddr_in *)&ss)->sin_addr;
                    return 1;
                case AF_INET6:
                    *(struct in6_addr *)dst = ((struct sockaddr_in6 *)&ss)->sin6_addr;
                    return 1;
            }
        }
        return 0;
    }

    // Functie om een binair IP-adres om te zetten naar tekstuele notatie
    const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
        struct sockaddr_storage ss;
        unsigned long s = size;

        ZeroMemory(&ss, sizeof(ss));
        ss.ss_family = af;

        switch(af) {
            case AF_INET:
                ((struct sockaddr_in *)&ss)->sin_addr = *(struct in_addr *)src;
                break;
            case AF_INET6:
                ((struct sockaddr_in6 *)&ss)->sin6_addr = *(struct in6_addr *)src;
                break;
            default:
                return NULL;
        }
        return (WSAAddressToString((struct sockaddr *)&ss, sizeof(ss), NULL, dst, &s) == 0) ? dst : NULL;
    }

    // Definitie van een struct voor clientinformatie
    typedef struct {
        SOCKET socket;
        char ip[INET6_ADDRSTRLEN];
    } client_info;

    // Functie om een client te bedienen (threadfunctie)
    DWORD WINAPI handleClient(LPVOID arg) {
        client_info *client = (client_info *)arg;
        SOCKET client_socket = client->socket;
        char *client_ip = client->ip;

        char buffer[2000];
        int bytes_received = recv(client_socket, buffer, sizeof buffer - 1, 0);

        if (bytes_received == -1) {
            reportError("recv mislukt");
        } else {
            buffer[bytes_received] = '\0';
            printf("Ontvangen: %s\n", buffer);

            // Loggen van clientgegevens naar een bestand
            FILE *log_file = fopen("client_log.txt", "a");
            if (log_file != NULL) {
                fprintf(log_file, "IP-adres: %s\nOntvangen gegevens: %s\n\n", client_ip, buffer);
                fclose(log_file);
            } else {
                perror("fopen mislukt");
            }

            // Loggen van geolocatiegegevens
            char command[256];
            snprintf(command, sizeof(command), "curl -s http://ip-api.com/json/%s", client_ip);
            FILE *fp = popen(command, "r");
            if (fp == NULL) {
                perror("popen mislukt");
            } else {
                char response[1000] = {0};
                fread(response, 1, sizeof(response) - 1, fp);
                log_file = fopen("client_log.txt", "a");
                if (log_file != NULL) {
                    fprintf(log_file, "Geolocatiegegevens voor IP: %s\n%s\n\n", client_ip, response);
                    fclose(log_file);
                } else {
                    perror("fopen mislukt");
                }
                pclose(fp);
            }

            // Voorbeeldrespons naar de client sturen
            const char *response = "Hallo, dit is een test\n";
            int bytes_sent_total = 0;
            for (int i = 0; i < 1000; ++i) {
                int bytes_sent = send(client_socket, response, strlen(response), 0);
                if (bytes_sent == -1) {
                    reportError("send failed");
                    break;
                }
                bytes_sent_total += bytes_sent;
                printf("Verzonden: %s\n", response);
            }

            // Loggen van het totale aantal verzonden bytes
            log_file = fopen("client_log.txt", "a");
            if (log_file != NULL) {
                fprintf(log_file, "Totaal aantal afgeleverde gegevens: %d\n\n", bytes_sent_total);
                fclose(log_file);
            } else {
                perror("fopen mislukt");
            }
        }

        // Socket sluiten en geheugen vrijgeven
        shutdown(client_socket, SHUT_RDWR);
        CLOSESOCKET(client_socket);
        free(client);
        return 0;
    }

#else
    // Unix-specifieke headers
    #include <pthread.h>
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
   
    #include <errno.h>
    #include <unistd.h>
    #include <stdint.h>

    // Functie om Windows sockets te initialiseren (dummyfunctie voor Unix)
    void initializeWindowsSockets() {}

    // Functie om Windows sockets op te schonen (dummyfunctie voor Unix)
    void cleanupWindowsSockets() {}

    // Macro om fouten te rapporteren
    #define reportError(msg) perror(msg)
    // Macro voor het sluiten van een socket
    #define CLOSESOCKET close

    // Definitie van een struct voor clientinformatie
    typedef struct {
        int socket;
        char ip[INET6_ADDRSTRLEN];
    } client_info;

    // Functie om een client te bedienen
    void *handleClient(void *arg) {
        client_info *client = (client_info *)arg;
        int client_socket = client->socket;
        char *client_ip = client->ip;

        char buffer[2000];
        int bytes_received = recv(client_socket, buffer, sizeof buffer - 1, 0);

        if (bytes_received == -1) {
            reportError("recv mislukt");
        } else {
            buffer[bytes_received] = '\0';
            printf("Ontvangen: %s\n", buffer);

            // Loggen van clientgegevens naar een bestand
            FILE *log_file = fopen("client_log.txt", "a");
            if (log_file != NULL) {
                fprintf(log_file, "IP-adres: %s\nOntvangen gegevens: %s\n\n", client_ip, buffer);
                fclose(log_file);
            } else {
                perror("fopen mislukt");
            }

            // Loggen van geolocatiegegevens
            char command[256];
            snprintf(command, sizeof(command), "curl -s http://ip-api.com/json/%s", client_ip);
            FILE *fp = popen(command, "r");
            if (fp == NULL) {
                perror("popen mislukt");
            } else {
                char response[1000] = {0};
                fread(response, 1, sizeof(response) - 1, fp);
                log_file = fopen("client_log.txt", "a");
                if (log_file != NULL) {
                    fprintf(log_file, "Geolocatiegegevens voor IP: %s\n%s\n\n", client_ip, response);
                    fclose(log_file);
                } else {
                    perror("fopen mislukt");
                }
                pclose(fp);
            }

            // Voorbeeldrespons naar de client sturen
            const char *response = "Hallo, dit is een test\n";
            int bytes_sent_total = 0;
            for (int i = 0; i < 1000; ++i) {
                int bytes_sent = send(client_socket, response, strlen(response), 0);
                if (bytes_sent == -1) {
                    reportError("send mislukt");
                    break;
                }
                bytes_sent_total += bytes_sent;
                printf("Verzonden: %s\n", response);
            }

            // Loggen van het totale aantal verzonden bytes
            log_file = fopen("client_log.txt", "a");
            if (log_file != NULL) {
                fprintf(log_file, "Totaal aantal verzonden bytes: %d\n\n", bytes_sent_total);
                fclose(log_file);
            } else {
                perror("fopen mislukt");
            }
        }

        // Socket sluiten en geheugen vrijgeven
        shutdown(client_socket, SHUT_RDWR);
        CLOSESOCKET(client_socket);
        free(client);
        return NULL;
    }
#endif

// Definitie van enkele constanten
#define API_URL "http://ip-api.com/json/"
#define PORT "22"
#define BACKLOG 10

// Functie om de server socket te initialiseren
int initServerSocket();
// Functie om een clientverbinding te accepteren
int acceptClientConnection(int server_socket);

// Hoofdprogramma
int main(int argc, char *argv[]) {
    // Initialisatie van Windows sockets (alleen voor Windows)
    initializeWindowsSockets();

    // Initialisatie van de server socket
    int server_socket = initServerSocket();
    printf("Luisteren op poort %s...\n", PORT);

    // Variabele voor threadhandvat (alleen voor Windows)
    #ifdef _WIN32
        HANDLE thread_handle;
    // Variabele voor thread-ID (voor Unix)
    #else
        pthread_t tid;
    #endif

    // Hoofdloop voor het accepteren van verbindingen
    while (1) {
        // Accepteren van een clientverbinding
        client_info *client = malloc(sizeof(client_info));
        if (!client) {
            perror("malloc mislukt");
            exit(EXIT_FAILURE);
        }

        client->socket = acceptClientConnection(server_socket);
        struct sockaddr_storage client_addr;
        socklen_t addr_size = sizeof client_addr;
        getpeername(client->socket, (struct sockaddr *)&client_addr, &addr_size);

        // Extractie van het IP-adres van de client
        if (client_addr.ss_family == AF_INET) {
            inet_ntop(AF_INET, &((struct sockaddr_in*)&client_addr)->sin_addr, client->ip, sizeof client->ip);
        } else {
            inet_ntop(AF_INET6, &((struct sockaddr_in6*)&client_addr)->sin6_addr, client->ip, sizeof client->ip);
        }

        printf("Verbonden met client met IP: %s\n", client->ip);

        // Creëren van een thread of proces om de client te bedienen, afhankelijk van het besturingssysteem
        #ifdef _WIN32
            thread_handle = CreateThread(NULL, 0, handleClient, client, 0, NULL);
            if (thread_handle == NULL) {
                reportError("CreateThread mislukt");
                free(client);
            } else {
                CloseHandle(thread_handle);
            }
        #else
            pthread_create(&tid, NULL, handleClient, client);
            pthread_detach(tid);
        #endif
    }

    // Sluiten van de server socket en opschonen van resources
    CLOSESOCKET(server_socket);
    cleanupWindowsSockets();
    return 0;
}

// Functie voor het initialiseren van de server socket
int initServerSocket() {
    struct addrinfo hints, *res, *p;
    int server_socket = -1;

    // Initialisatie van hints struct
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Resolutie van adresinformatie
    if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
        fprintf(stderr, "getaddrinfo mislukt\n");
        exit(EXIT_FAILURE);
    }

    // Loop door verkregen adressen om een bruikbaar adres te vinden
    for (p = res; p != NULL; p = p->ai_next) {
        // Creatie van een socket
        server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (server_socket == -1) {
            reportError("socket creatie mislukt");
            continue;
        }

        // Binding van de socket aan een adres
        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            reportError("bind mislukt");
            CLOSESOCKET(server_socket);
            continue;
        }

        // Luisteren op de socket voor inkomende verbindingen
        if (listen(server_socket, BACKLOG) == -1) {
            reportError("luisteren mislukt");
            CLOSESOCKET(server_socket);
            continue;
        }

        // Onderbreken van de loop als alles succesvol is
        break;
    }

    // Vrijgeven van de adresinformatie
    freeaddrinfo(res);

    // Afhandeling van fouten bij het verkrijgen van een bruikbaar adres
    if (p == NULL) {
        fprintf(stderr, "Niet gelukt om een adres te binden\n");
        exit(EXIT_FAILURE);
    }

    return server_socket;
}

// Functie voor het accepteren van een clientverbinding
int acceptClientConnection(int server_socket) {
    struct sockaddr_storage client_addr;
    socklen_t addr_size = sizeof client_addr;
    // Accepteren van de verbinding
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);

    // Afhandeling van fouten bij accepteren van verbinding
    if (client_socket == -1) {
        reportError("accept mislukt");
        CLOSESOCKET(server_socket);
        exit(EXIT_FAILURE);
    }

    return client_socket;
}
