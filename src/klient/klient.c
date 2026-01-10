#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdbool.h>

#define PORT 12345
#define MAX_BUFFER 1024

/*
 * Klient pre komunikáciu so serverom simulácie.
 * - posiela príkazy NEW_SIM / SET_MODE / STOP_SIM
 * - prijíma a spracováva riadkové správy od servera
 */

void print_main_menu() {
    printf("\n=== Random Walk Client ===\n");
    printf("1 - New Simulation\n");
    printf("2 - Join Existing Simulation\n");
    printf("3 - Replay Saved Simulation\n");
    printf("4 - Quit\n");
    printf("Choice: ");
}

/* Vytvorí TCP spojenie na lokálny server a vráti socket alebo -1 pri chybe */
int connect_to_server() {
    int sock;
    struct sockaddr_in serv_addr;
    int attempts = 5;
    for (int a = 0; a < attempts; ++a) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket");
            return -1;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);
        inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
            return sock;
        }

        close(sock);
        if (a < attempts - 1) {
            sleep(1); /* wait for server to start */
        }
    }

    /* final attempt failed */
    return -1;
}

/* AI Metoda */
int spawn_server() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        /* child */
        /* try common relative/system paths for the server binary */
        execl("./server", "server", NULL);
        execl("../server/server", "server", NULL);
        execl("/usr/local/bin/server", "server", NULL);
        execl("/usr/bin/server", "server", NULL);
        perror("execl server");
        _exit(127);
    }
    /* parent: avoid zombie children */
    signal(SIGCHLD, SIG_IGN);
    /* give server a moment to bind/listen */
    sleep(1);
    return pid;
}
//AI metoda na zistenie rozmerov mapy z fileu
void calculate_map_size(const char* filename, int* out_width, int* out_height) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("fopen");
        *out_width = 0;
        *out_height = 0;
        return;
    }

    int width = 0;
    int height = 0;
    int current_width = 0;
    int c;

    while ((c = fgetc(file)) != EOF) {
        if (c == '0' || c == '1') {
            current_width++;
        } else if (c == '\n') {
            if (current_width > width) {
                width = current_width;
            }
            current_width = 0;
            height++;
        }
    }
    // Check for last line without newline
    if (current_width > 0) {
        if (current_width > width) {
            width = current_width;
        }
        height++;
    }

    fclose(file);
    *out_width = width;
    *out_height = height;
}

// AI  - spracude správy zo servera a vypíše ich na obrazovku
void process_server(int sock, int world_width, int world_height, int mod) {
    char buffer[MAX_BUFFER];
    char recv_accum[8192];
    int accum_len = 0;
    bool in_traj = false;
    char traj_buf[4096]; traj_buf[0] = '\0';

    memset(recv_accum, 0, sizeof(recv_accum));

    while (1) {
        /* Používame select() na čítanie zo servera aj príkazov od používateľa (stdin) */
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        FD_SET(STDIN_FILENO, &rfds);
        int maxfd = sock > STDIN_FILENO ? sock : STDIN_FILENO;
        int sel = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (sel < 0) { perror("select"); break; }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            /* Spracovanie lokálnych príkazov používateľa (mode/stop/quit/help) */
            char line_in[256];
            if (!fgets(line_in, sizeof(line_in), stdin)) { /* EOF */ }
            size_t L = strcspn(line_in, "\r\n"); line_in[L] = '\0';

            if (strcmp(line_in, "quit") == 0 || strcmp(line_in, "exit") == 0) {
                printf("Disconnecting from server...\n");
                close(sock);
                return;
            }

             if (strncmp(line_in, "mode ", 5) == 0) {
                int newm = atoi(line_in + 5);
                if (newm == 1 || newm == 2) {
                    char out[64]; snprintf(out, sizeof(out), "SET_MODE %d\n", newm);
                    send(sock, out, strlen(out), 0);
                    /* Lokálny `mod` nemeníme tu — čakáme na potvrdenie "MODE <n>" zo servera */
                    printf("Režim %d požiadavka odoslaná — čakám na potvrdenie zo servera\n", newm);
                } else printf("Invalid mode (1 or 2)\n");
            } else if (strcmp(line_in, "stop") == 0) {
                send(sock, "STOP_SIM\n", 9, 0);
                printf("Stop requested.\n");
            } else if (strcmp(line_in, "help") == 0) {
                printf("Commands: mode 1 | mode 2 | stop | quit | help\n");
            } else {
                printf("Unknown command. Type 'help'.\n");
            }
        }

        if (FD_ISSET(sock, &rfds)) {
            /* Prijaté bajty od servera: agregujeme do vnútorného akumulátora a delíme podľa '\n' */
            int n = read(sock, buffer, sizeof(buffer)-1);
            if (n <= 0) { printf("Server disconnected.\n"); close(sock); return; }
            if (accum_len + n >= (int)sizeof(recv_accum)) { accum_len = 0; memset(recv_accum,0,sizeof(recv_accum)); }
            memcpy(recv_accum + accum_len, buffer, n); accum_len += n;

            while (1) {
                char *nl = memchr(recv_accum, '\n', accum_len);
                if (!nl) break;
                int line_len = nl - recv_accum;
                char line[2048]; if (line_len >= (int)sizeof(line)) line_len = sizeof(line)-1;
                memcpy(line, recv_accum, line_len); line[line_len] = '\0';
                int remaining = accum_len - (line_len + 1);
                if (remaining > 0) memmove(recv_accum, nl+1, remaining);
                accum_len = remaining;
                if (line_len > 0 && line[line_len-1] == '\r') line[line_len-1] = '\0';

                if (strncmp(line, "MODE", 4) == 0) {
                    int server_mode = 0; if (sscanf(line, "MODE %d", &server_mode) == 1) { mod = server_mode; printf("Mode switched to %d by server\n", mod); }
                }
                else if (strncmp(line, "SUMMARY", 7) == 0) {
                    int x,y; double avg, prob; if (sscanf(line, "SUMMARY %d %d %lf %lf", &x, &y, &avg, &prob) >= 4) {
                        printf("Cell (%d,%d): avg=", x, y);
                        if (avg < 0.0) printf("- "); else printf("%.3f ", avg);
                        printf("prob=%.3f\n", prob);
                    }
                }
                else if (strncmp(line, "SUMMARY_DONE", 12) == 0) {
                    printf("Summary complete.\n");
                    /* do not return — keep connection open so user can switch modes or run again */
                }
                else if (mod == 2 && strncmp(line, "START", 5) == 0) {
                    int sx,sy,rep; if (sscanf(line, "START %d %d %d", &sx, &sy, &rep) == 3) {
                        in_traj = true;
                        snprintf(traj_buf, sizeof(traj_buf), "Replication %d | Start (%d,%d)", rep, sx, sy);
                    }
                }
                else if (mod == 2 && strncmp(line, "POS", 3) == 0) {
                    int x,y; if (sscanf(line, "POS %d %d", &x, &y) == 2 && in_traj) {
                        strncat(traj_buf, " -> ", sizeof(traj_buf)-strlen(traj_buf)-1);
                        char tmp[64]; snprintf(tmp,sizeof(tmp),"(%d,%d)",x,y);
                        strncat(traj_buf,tmp,sizeof(traj_buf)-strlen(traj_buf)-1);
                    }
                }
                else if (mod == 2 && strncmp(line, "END", 3) == 0) {
                    int total_steps, hit_center; if (sscanf(line, "END %d %d", &total_steps, &hit_center) == 2) {
                        if (in_traj) {
                            printf("%s | Steps: %d | Hit center: %s\n", traj_buf, total_steps, hit_center?"YES":"NO");
                            in_traj = false;
                        } else {
                            printf("End: Steps: %d | Hit center: %s\n", total_steps, hit_center?"YES":"NO");
                        }
                    }
                }
                else {
                    if (line[0] != '\0') printf("%s\n", line);
                }
            }
        }
    }
}

int main() {
    int sock = -1;
    int current_start_x = 0;
    int current_start_y = 0;
    int current_rep = 0;
    int steps = 0;
    int world_height, world_width, obstacles;
    char buffer[MAX_BUFFER];
    int mod;
    char obstacles_filename[128] = "";

    while (1) {
        print_main_menu();
        int choice;
        scanf("%d", &choice);
        getchar(); // odstráni enter po scanf

        if (choice == 1 || choice == 2) {
            if (choice == 1) {
                /* spawn local server for New Simulation */
                if (spawn_server() < 0) {
                    printf("Warning: failed to spawn local server; will still try to connect.\n");
                }
            }

            sock = connect_to_server();   //  ulož výsledok
            if (sock < 0) { printf("Could not connect to server.\n"); continue; }

            int width, height, K, replications;
            int obstacles, mode;
            float p_up, p_down, p_left, p_right;
            char filename[128];

           printf("Add obstacles? (0=no,1=yes): "); scanf("%d", &obstacles);
            if (obstacles == 1) {
                printf("Enter file with obstacles: ");
                scanf("%127s", obstacles_filename);
                 strncpy(obstacles_filename, "/mnt/c/Users/simon/mapa.txt", sizeof(obstacles_filename)-1);
                 obstacles_filename[sizeof(obstacles_filename)-1] = '\0';
            } else {
                obstacles_filename[0] = '\0';
            }
            if(obstacles == 0) {
                printf("Enter world width: "); scanf("%d", &width);
                printf("Enter world height: "); scanf("%d", &height);
            } else {
                calculate_map_size(obstacles_filename, &width, &height);
            }

            printf("Enter maximum steps K: "); scanf("%d", &K);
            printf("Enter number of replications: "); scanf("%d", &replications);
            
            printf("Enter the mode you want 1=summary, 2=innteractive ");scanf("%d", &mode);
            printf("Enter movement probabilities (up down left right), sum=1.0: \n");
            scanf("%f %f %f %f", &p_up, &p_down, &p_left, &p_right);
            while((p_up + p_down + p_left + p_right) != 1.0) {
                printf("Probabilities must sum to 1.0. Please re-enter:\n");
                scanf("%f %f %f %f", &p_up, &p_down, &p_left, &p_right);
            }
            getchar();
            printf("Enter output file name: "); fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = 0;

               /* send obstacles filename (or '-' placeholder) and output filename as last tokens */
               char obs_token[128];
               if (obstacles == 1 && obstacles_filename[0] != '\0') {
                   strncpy(obs_token, obstacles_filename, sizeof(obs_token)-1);
                   obs_token[sizeof(obs_token)-1] = '\0';
               } else {
                   /* send a placeholder so server's sscanf keeps tokens aligned */
                   strncpy(obs_token, "-", sizeof(obs_token)-1);
                   obs_token[sizeof(obs_token)-1] = '\0';
               }

               snprintf(buffer, sizeof(buffer),
                   "NEW_SIM %d %d %d %d %d %d %.2f %.2f %.2f %.2f %127s %127s\n",
                   width, height, K, replications, obstacles, mode,
                   p_up, p_down, p_left, p_right, obs_token, filename);

            send(sock, buffer, strlen(buffer), 0);
            mod = mode;
            world_height = height;
            world_width = width;
            printf("New simulation parameters sent to server.\n");
            

            process_server(sock, width, height, mode);
            /* process_server uzatvorí socket pri quit / odpojení; už nevoláme close() tu */
            sock = -1;
            continue;
        }
    
    return 0;
    }
}