#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <stdatomic.h>
#include <signal.h>

#include "../world/world.h"
#include "../walker/walker.h"
#include "../simulation/simulation.h"

#include "server.h"

int main() {
    /* Inicializácia generátora náhodných čísel */
    srand(time(NULL));

    /* AI CHAT GPT - Ignoruj SIGPIPE – aby server nespadol,
       keď sa pokúsi zapisovať do zatvoreného socketu */
    signal(SIGPIPE, SIG_IGN);

    /* File deskriptory server socketu a klienta */
    int server_fd = -1, client_sock = -1;

    /* Príznak, ktorý určuje, či sa má server po simulácii ukončiť */
    int exit_after_simulation = 0;

    /* Štruktúra pre adresu servera */
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    /* Buffer na prijímanie správ od klienta */
    char buffer[MAX_BUFFER];

    /* Vytvorenie TCP socketu */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }

    /* AI  - Chat gpt Povolenie opätovného použitia adresy (aby port nezostal blokovaný) */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Nastavenie adresy servera */
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    /* Priradenie socketu k adrese a portu */
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }

    /* Server začne počúvať na porte */
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    /* AI - Chat GPT  Nastavenie server socketu na non-blocking mód
       (accept nebude blokovať, keď nie je klient) */
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    /* Aktuálne bežiaca simulácia */
    Simulation *current_sim = NULL;

    /* Hlavná slučka servera – beží, kým nie je exit_after_simulation */
    while (!exit_after_simulation) {

        /* Pokus o prijatie klienta */
        client_sock = accept(server_fd,
                             (struct sockaddr *)&address,
                             (socklen_t*)&addrlen);

        if (client_sock < 0) {
            /* Idea od chatgpt  - Ak momentálne nie je klient, iba počkáme */
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000); // 0.1 sekundy
                continue;
            } else {
                perror("accept");
                break;
            }
        }

        printf("Client connected, fd=%d\n", client_sock);

        /* Slučka na spracovanie príkazov od klienta */
        while (1) {

            /* Čítanie dát od klienta */
            ssize_t n = read(client_sock, buffer, sizeof(buffer) - 1);
            if (n <= 0) {
                /* Klient sa odpojil alebo nastala chyba */
                printf("client disconnected or read error\n");
                close(client_sock);
                client_sock = -1;
                break;
            }

            buffer[n] = '\0';

            /* ====== NEW_SIM – spustenie novej simulácie ====== */
            if (strncmp(buffer, "NEW_SIM", 7) == 0) {

                printf("Received command: %s", buffer);

                /* Parametre simulácie */
                int width, height, K, replications, obstacles, mode;
                float pu, pd, pl, pr;
                char obs_filename[128] = {0};
                char out_filename[128] = {0};

                /* Parsovanie príkazu */
                int scanned = sscanf(
                    buffer,
                    "NEW_SIM %d %d %d %d %d %d %f %f %f %f %127s %127s",
                    &width, &height, &K, &replications,
                    &obstacles, &mode,
                    &pu, &pd, &pl, &pr,
                    obs_filename, out_filename
                );

                if (scanned < 11) {
                    send(client_sock, "ERROR bad_NEW_SIM\n", 18, 0);
                    continue;
                }

                /* Zrušenie starej simulácie */
                if (current_sim) {
                    simulation_destroy(current_sim);
                    current_sim = NULL;
                }

                /* Vytvorenie novej simulácie */
                current_sim = simulation_create(
                    width, height, obstacles, K, replications,
                    pu, pd, pl, pr,
                    mode == 2,
                    client_sock
                );

                if (!current_sim) {
                    send(client_sock, "ERROR simulation_create\n", 23, 0);
                    continue;
                }

                /* Načítanie prekážok zo súboru */
                if (obstacles == 1 && obs_filename[0] != '\0') {
                    read_file_with_obstacles(current_sim->world, obs_filename);
                }

                /* Spustenie výpočtu simulácie */
                simulation_run(current_sim);

                /* AI - Chat GPT Informovanie klienta o móde */
                char ack[32];
                snprintf(ack, sizeof(ack), "MODE %d\n", mode);
                send(client_sock, ack, strlen(ack), 0);

                /* Odoslanie výsledkov podľa módu */
                if (mode == 1) {
                    simulation_send_summary(current_sim);
                    send(client_sock, "SUMMARY_DONE\n", 13, 0);
                } else {
                    simulation_send_interactive(current_sim);
                }

                /* Uloženie výsledkov do súboru (ak bol zadaný) */
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
                                    fprintf(f, "SUMMARY %d %d -1.0 0.0\n", x, y);
                                    continue;
                                }

                                if (x == 0 && y == 0) {
                                    fprintf(f, "SUMMARY %d %d 0.0 1.0\n", x, y);
                                    continue;
                                }

                                double avg = -1.0;
                                if (current_sim->success_counts[idx] > 0) {
                                    avg = current_sim->sum_steps[idx] /
                                          (double)current_sim->success_counts[idx];
                                }

                                double prob =
                                    (double)current_sim->success_counts[idx] /
                                    (double)reps;

                                fprintf(f, "SUMMARY %d %d %.6f %.6f\n",
                                        x, y, avg, prob);
                            }
                        }
                        fclose(f);
                    }
                }

                /* GITHUB COPILOT zväčša Po dokončení simulácie sa server ukončí */
                exit_after_simulation = 1;
                shutdown(client_sock, SHUT_RDWR);
                close(client_sock);
                client_sock = -1;
                break;
            }

            /* ====== STOP_SIM – zastavenie bežiacej simulácie ====== */
            else if (strncmp(buffer, "STOP_SIM", 8) == 0) {
                if (current_sim) {
                    atomic_store(&current_sim->stop_requested, 1);
                    send(client_sock, "STOP_OK\n", 8, 0);
                } else {
                    send(client_sock, "ERROR no_simulation\n", 20, 0);
                }
            }

            /* ====== QUIT – klient sa odpojí ====== */
            else if (strncmp(buffer, "QUIT", 4) == 0) {
                close(client_sock);
                client_sock = -1;
                break;
            }

            /* ====== Neznámy príkaz ====== */
            else {
                send(client_sock, "UNKNOWN_CMD\n", 12, 0);
            }
        }
    }

    /* Upratanie zdrojov */
    if (current_sim)
        simulation_destroy(current_sim);

    close(server_fd);
    printf("Server exiting.\n");
    return 0;
}
