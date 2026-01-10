#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <stdatomic.h>

#include "../world/world.h"
#include "../walker/walker.h"
#include "../simulation/simulation.h"

#define PORT 12345
#define MAX_CLIENTS 1
#define MAX_BUFFER 1024

int main() {
    srand(time(NULL));

    int server_fd = -1, client_sock = -1;
    int exit_after_simulation = 0;
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

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    // nastav server_fd na non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    Simulation *current_sim = NULL;

    while (!exit_after_simulation) {
        client_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_sock < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // žiadne pripojenie, počkaj trošku
                usleep(100000); // 0.1 sekundy
                continue;
            } else {
                perror("accept");
                break;
            }
        }

        printf("Client connected, fd=%d\n", client_sock);

        while (1) {
            ssize_t n = read(client_sock, buffer, sizeof(buffer)-1);
            if (n <= 0) {
                printf("client disconnected or read error\n");
                close(client_sock);
                client_sock = -1;
                break;
            }
            buffer[n] = '\0';

            if (strncmp(buffer, "NEW_SIM", 7) == 0) {
                printf("Received command: %s", buffer); fflush(stdout);
                int width, height, K, replications, obstacles, mode;
                float pu, pd, pl, pr;
                char obs_filename[128] = {0};
                char out_filename[128] = {0};

                int scanned = sscanf(buffer, "NEW_SIM %d %d %d %d %d %d %f %f %f %f %127s %127s",
                                     &width, &height, &K, &replications, &obstacles, &mode,
                                     &pu, &pd, &pl, &pr, obs_filename, out_filename);

                if (scanned < 11) {
                    send(client_sock, "ERROR bad_NEW_SIM\n", 18, 0);
                    continue;
                }

                printf("Parsed NEW_SIM: width=%d height=%d K=%d reps=%d obstacles=%d mode=%d\n", width, height, K, replications, obstacles, mode);
                printf("obs_filename='%s' out_filename='%s'\n", obs_filename, out_filename); fflush(stdout);

                if (current_sim) { simulation_destroy(current_sim); current_sim = NULL; }

                current_sim = simulation_create(width, height, obstacles, K, replications, pu, pd, pl, pr, mode==2, client_sock);
                if (!current_sim) {
                    send(client_sock, "ERROR simulation_create\n", 23, 0);
                    continue;
                }

                if (obstacles == 1 && obs_filename[0] != '\0') {
                    read_file_with_obstacles(current_sim->world, obs_filename);
                }

                printf("Starting simulation_run()...\n"); fflush(stdout);
                simulation_run(current_sim);
                printf("simulation_run() returned\n"); fflush(stdout);

                char ack[32];
                snprintf(ack, sizeof(ack), "MODE %d\n", mode);
                send(client_sock, ack, strlen(ack), 0);

                if (mode == 1) {
                    simulation_send_summary(current_sim, client_sock);
                    send(client_sock, "SUMMARY_DONE\n", strlen("SUMMARY_DONE\n"), 0);
                } else {
                    simulation_send_interactive(current_sim, client_sock);
                }

                /* Ak používateľ zadal výstupný súbor, uložíme sumarizáciu aj doňho */
                if (out_filename[0] != '\0') {
                    FILE *f = fopen(out_filename, "w");
                    if (f) {
                        int w = current_sim->world->width;
                        int h = current_sim->world->height;
                        int reps = current_sim->replications;
                        for (int y = 0; y < h; ++y) {
                            for (int x = 0; x < w; ++x) {
                                int idx = y * w + x;
                                if (world_is_obstacle(current_sim->world, x, y)) {
                                    fprintf(f, "SUMMARY %d %d %f %f\n", x, y, -1.0, 0.0);
                                    continue;
                                }
                                if (x == 0 && y == 0) {
                                    fprintf(f, "SUMMARY %d %d %f %f\n", x, y, 0.0, 1.0);
                                    continue;
                                }
                                double avg = -1.0;
                                if (current_sim->success_counts[idx] > 0) avg = current_sim->sum_steps[idx] / (double)current_sim->success_counts[idx];
                                double prob = (double)current_sim->success_counts[idx] / (double)reps;
                                fprintf(f, "SUMMARY %d %d %.6f %.6f\n", x, y, avg, prob);
                            }
                        }
                        fclose(f);
                    } else {
                        char errbuf[256]; snprintf(errbuf, sizeof(errbuf), "ERROR writing_output %s\n", out_filename);
                        send(client_sock, errbuf, strlen(errbuf), 0);
                    }
                }

                // po dokončení simulácie sa server ukončí
                printf("Sending results complete, shutting down client socket and exiting main loop\n"); fflush(stdout);
                exit_after_simulation = 1;
                /* ensure peer sees EOF and unblock any blocked send/recv */
                shutdown(client_sock, SHUT_RDWR);
                close(client_sock);
                client_sock = -1;
                break;

            } else if (strncmp(buffer, "SET_MODE", 8) == 0) {
                if (!current_sim) { send(client_sock, "ERROR no_simulation\n", 19, 0); continue; }
                int newmode = 0;
                if (sscanf(buffer, "SET_MODE %d", &newmode) == 1 && (newmode == 1 || newmode == 2)) {
                    char ack[32];
                    snprintf(ack, sizeof(ack), "MODE %d\n", newmode);
                    send(client_sock, ack, strlen(ack), 0);

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
                if (current_sim) {
                    atomic_store(&current_sim->stop_requested, 1);
                    send(client_sock, "STOP_OK\n", 8, 0);
                } else {
                    send(client_sock, "ERROR no_simulation\n", 20, 0);
                }
            } else if (strncmp(buffer, "QUIT", 4) == 0) {
                printf("client requested quit\n");
                close(client_sock);
                client_sock = -1;
                break;
            } else {
                send(client_sock, "UNKNOWN_CMD\n", 12, 0);
            }
        }

        if (client_sock != -1) close(client_sock);
        client_sock = -1;
    }

    if (current_sim) simulation_destroy(current_sim);
    close(server_fd);
    printf("Server exiting.\n");
    return 0;
}
