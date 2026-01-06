#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#include "../world/world.h"
#include "../walker/walker.h"
#include "../simulation/simulation.h"

#define PORT 12345
#define MAX_CLIENTS 5
#define MAX_BUFFER 1024

int main() {
    srand(time(NULL));

    int server_fd = -1, client_sock = -1;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[MAX_BUFFER];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket failed"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("bind failed"); close(server_fd); return 1; }
    if (listen(server_fd, MAX_CLIENTS) < 0) { perror("listen"); close(server_fd); return 1; }

    printf("Server listening on port %d...\n", PORT);

    Simulation *current_sim = NULL;

    while (1) {
        client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }
        printf("Client connected, fd=%d\n", client_sock);

        while (1) {
            ssize_t n = read(client_sock, buffer, sizeof(buffer)-1);
            if (n <= 0) { printf("client disconnected or read error\n"); close(client_sock); break; }
            buffer[n] = '\0';

            if (strncmp(buffer, "NEW_SIM", 7) == 0) {
                int width, height, K, replications, obstacles, mode;
                float pu, pd, pl, pr;
                char filename[128] = {0};

                int scanned = sscanf(buffer, "NEW_SIM %d %d %d %d %d %d %f %f %f %f %127s",
                           &width, &height, &K, &replications, &obstacles, &mode,
                           &pu, &pd, &pl, &pr, filename);
                if (scanned < 10) {
                    send(client_sock, "ERROR bad_NEW_SIM\n", 18, 0);
                    continue;
                }

                if (current_sim) { simulation_destroy(current_sim); current_sim = NULL; }

                current_sim = simulation_create(width, height, obstacles, K, replications, pu, pd, pl, pr, mode==2, client_sock);
                if (!current_sim) {
                    send(client_sock, "ERROR simulation_create\n", 23, 0);
                    continue;
                }

                /* Ak bol zadaný súbor s prekážkami, načítaj ho do sveta */
                if (obstacles == 1 && filename[0] != '\0') {
                    read_file_with_obstacles(current_sim->world, filename);
                }

                /* spusti jednu simuláciu (vnútri môže bežať viac vlákien na zber štatisík) */
                simulation_run(current_sim);

                /* pošlite počiatočný pohľad podľa požadovaného módu: ACK najskôr, aby klient prepol lokálny mód */
                char ack[32];
                snprintf(ack, sizeof(ack), "MODE %d\n", mode);
                send(client_sock, ack, strlen(ack), 0);

                if (mode == 1) {
                    simulation_send_summary(current_sim, client_sock);
                    send(client_sock, "SUMMARY_DONE\n", strlen("SUMMARY_DONE\n"), 0);
                } else {
                    simulation_send_interactive(current_sim, client_sock);
                }

            } else if (strncmp(buffer, "SET_MODE", 8) == 0) {
                if (!current_sim) { send(client_sock, "ERROR no_simulation\n", 19, 0); continue; }
                int newmode = 0;
                if (sscanf(buffer, "SET_MODE %d", &newmode) == 1 && (newmode == 1 || newmode == 2)) {
                    /* potvrdenie najprv (ACK), potom pošleme príslušné dáta klientovi */
                    char ack[32]; snprintf(ack, sizeof(ack), "MODE %d\n", newmode); send(client_sock, ack, strlen(ack), 0);

                    if (newmode == 1) {
                        simulation_send_summary(current_sim, client_sock);
                        send(client_sock, "SUMMARY_DONE\n", strlen("SUMMARY_DONE\n"), 0);
                    } else {
                        simulation_send_interactive(current_sim, client_sock);
                    }
                } else {
                    send(client_sock, "ERROR invalid_mode\n", 19, 0);
                }
            } else if (strncmp(buffer, "STOP_SIM", 8) == 0) {
                /* príkaz na zastavenie simulácie počas behu: nastavíme stop flag */
                if (current_sim) {
                    atomic_store(&current_sim->stop_requested, 1);
                    send(client_sock, "STOP_OK\n", 8, 0);
                } else {
                    send(client_sock, "ERROR no_simulation\n", 20, 0);
                }
            } else if (strncmp(buffer, "QUIT", 4) == 0) {
                printf("client requested quit\n");
                close(client_sock);
                break;
            } else {
                send(client_sock, "UNKNOWN_CMD\n", 12, 0);
            }
        }

        close(client_sock);
    }

    if (current_sim) simulation_destroy(current_sim);
    close(server_fd);
    return 0;
}
