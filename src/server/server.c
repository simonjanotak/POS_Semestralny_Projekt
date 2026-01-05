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

    int server_fd, client_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[MAX_BUFFER];

    // socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) { perror("socket failed"); exit(EXIT_FAILURE); }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { perror("bind failed"); exit(EXIT_FAILURE); }
    if (listen(server_fd, MAX_CLIENTS) < 0) { perror("listen"); exit(EXIT_FAILURE); }

     printf("Server listening on port %d...\n", PORT);

    while (1) {
        client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_sock < 0) { perror("accept"); continue; }

        printf("Client connected!\n");

        while (1) {
            int n = read(client_sock, buffer, sizeof(buffer)-1);
            if (n <= 0) { printf("Client disconnected.\n"); break; }
            buffer[n] = 0;

            if (strncmp(buffer, "NEW_SIM", 7) == 0) {
                int width, height, K, replications, obstacles, mode;
                float pu, pd, pl, pr;
                char filename[128];

                sscanf(buffer, "NEW_SIM %d %d %d %d %d %d %f %f %f %f %s",
                       &width, &height, &K, &replications, &obstacles, &mode,
                       &pu, &pd, &pl, &pr, filename);

                int interactive_flag = (mode == 2) ? 1 : 0;
                Simulation* sim = simulation_create(
                    width, height, obstacles, K, replications, pu, pd, pl, pr, interactive_flag, client_sock
                );
                if (!sim) {
                    send(client_sock, "ERROR simulation_create\n", 23, 0);
                } else {
                    simulation_run(sim);
                    simulation_destroy(sim);
                }
                /* after finishing a simulation, keep connection open for more commands */
            }
            else if (strncmp(buffer, "SET_MODE", 8) == 0) {
                /* ACK only — to fully change running simulation behavior, simulation_run must check for control messages */
                send(client_sock, "MODE_OK\n", 8, 0);
            }
            else if (strncmp(buffer, "STOP_SIM", 8) == 0) {
                /* ACK only — to actually stop a running simulation, simulation_run must support external stop */
                send(client_sock, "STOP_OK\n", 8, 0);
            }
            else if (strncmp(buffer, "QUIT", 4) == 0) {
                printf("Client requested quit.\n");
                break;
            }
            else {
                send(client_sock, "UNKNOWN_CMD\n", 12, 0);
            }
        }

        close(client_sock);
    }

    close(server_fd);
    return 0;
}
