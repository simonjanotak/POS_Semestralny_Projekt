#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "../world/world.h"
#include "../walker/walker.h"   // <--- tu voláme walker modul

#define PORT 12345
#define MAX_CLIENTS 5
#define MAX_BUFFER 1024

int main() {
    srand(time(NULL));

    int server_fd, client_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[MAX_BUFFER];

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

        int n = read(client_sock, buffer, sizeof(buffer)-1);
        if (n <= 0) { close(client_sock); continue; }
        buffer[n] = 0;

        if (strncmp(buffer, "NEW_SIM", 7) == 0) {
            int width, height, K, replications, obstacles, mode;
            float pu, pd, pl, pr;
            char filename[128];

            sscanf(buffer, "NEW_SIM %d %d %d %d %d %d %f %f %f %f %s",
                   &width, &height, &K, &replications, &obstacles, &mode,
                   &pu, &pd, &pl, &pr, filename);

            WorldType type = obstacles ? WORLD_WITH_OBSTACLES : WORLD_NO_OBSTACLES;
            World *w = world_create(width, height, type);

            if (type == WORLD_WITH_OBSTACLES) {
                int count = width * height / 5; // napr. 20% prekážok
                world_generate_obstacles(w, count);
            }

            // --- spusti walker pre každé voľné políčko ---
            for (int r = 0; r < replications; r++) {
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        if (!world_is_obstacle(w, x, y)) {
                            // vytvor walker
                            Walker wkr;
                            walker_init(&wkr, x, y);

                            // spusti pohyb, interaktívny mód=mode, K krokov
                            walker_simulate(&wkr, w, pu, pd, pl, pr, K, mode, client_sock);
                        }
                    }
                }
            }

            world_destroy(w);
        }
        else if (strncmp(buffer, "JOIN_SIM", 8) == 0) {
            send(client_sock, "JOIN SIM NOT IMPLEMENTED\n", 26, 0);
        }
        else if (strncmp(buffer, "REPLAY_SIM", 10) == 0) {
            send(client_sock, "REPLAY SIM NOT IMPLEMENTED\n", 28, 0);
        }
        else if (strncmp(buffer, "QUIT", 4) == 0) {
            printf("Client requested quit.\n");
        }

        close(client_sock);
    }

    close(server_fd);
    return 0;
}
