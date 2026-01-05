#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdbool.h>
#define PORT 12345
#define MAX_BUFFER 1024

void print_main_menu() {
    printf("\n=== Random Walk Client ===\n");
    printf("1 - New Simulation\n");
    printf("2 - Join Existing Simulation\n");
    printf("3 - Replay Saved Simulation\n");
    printf("4 - Quit\n");
    printf("Choice: ");
}

int connect_to_server() {
    int sock;
    struct sockaddr_in serv_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}

// AI  - spracude správy zo servera a vypíše ich na obrazovku
void process_server(int sock, int world_width, int world_height, int mod, int **obstacles_map) {
    char buffer[MAX_BUFFER];
    char recv_accum[8192];
    int accum_len = 0;
    bool in_traj = false;
    char traj_buf[4096]; traj_buf[0] = '\0';

    memset(recv_accum, 0, sizeof(recv_accum));

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        FD_SET(STDIN_FILENO, &rfds);
        int maxfd = sock > STDIN_FILENO ? sock : STDIN_FILENO;
        int sel = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (sel < 0) { perror("select"); break; }

        if (FD_ISSET(STDIN_FILENO, &rfds)) {
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
                    mod = newm;
                    printf("Requested mode %d\n", newm);
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
                else if (strncmp(line, "OBSTACLE", 8) == 0 && obstacles_map) {
                    int x,y; if (sscanf(line, "OBSTACLE %d %d", &x, &y) == 2) { if (x>=0 && x<world_width && y>=0 && y<world_height) obstacles_map[y][x]=1; }
                }
                else if (strncmp(line, "OBSTACLE_END", 12) == 0) {
                    printf("All obstacles received.\n");
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
    int** obstacles_map = NULL;
    int world_height, world_width, obstacles;
    char buffer[MAX_BUFFER];
    int mod;

    while (1) {
        print_main_menu();
        int choice;
        scanf("%d", &choice);
        getchar(); // odstráni enter po scanf

        if (choice == 1) {
        sock = connect_to_server();   //  ulož výsledok
        if (sock < 0) continue;       // ak sa nepripojil, vráť sa späť do menu

        int width, height, K, replications;
        int obstacles, mode;
        float p_up, p_down, p_left, p_right;
        char filename[128];
       

        printf("Enter world width: "); scanf("%d", &width);
        printf("Enter world height: "); scanf("%d", &height);
        printf("Enter maximum steps K: "); scanf("%d", &K);
        printf("Enter number of replications: "); scanf("%d", &replications);
        printf("Add obstacles? (0=no,1=yes): "); scanf("%d", &obstacles);
        printf("Enter the mode you want 1=summary, 2=innteractive");scanf("%d", &mode);
        printf("Enter movement probabilities (up down left right), sum=1.0:\n");
        scanf("%f %f %f %f", &p_up, &p_down, &p_left, &p_right);
        getchar();
        printf("Enter output file name: "); fgets(filename, sizeof(filename), stdin);
        filename[strcspn(filename, "\n")] = 0;

        snprintf(buffer, sizeof(buffer),
             "NEW_SIM %d %d %d %d %d %d %.2f %.2f %.2f %.2f %s\n",
             width, height, K, replications, obstacles, mode,
             p_up, p_down, p_left, p_right, filename);

        send(sock, buffer, strlen(buffer), 0);
        mod = mode;
        world_height = height;
        world_width = width;
        printf("New simulation parameters sent to server.\n");
        if (obstacles && obstacles_map == NULL) {
            obstacles_map = malloc(world_height * sizeof(int*));
            for (int i = 0; i < world_height; i++)
                obstacles_map[i] = calloc(world_width, sizeof(int));
        }

        process_server(sock, width, height, mode, obstacles_map);
        /* process_server uzatvorí socket pri quit / odpojení; už nevoláme close() tu */
        sock = -1;
        if (obstacles_map) {
            for (int i = 0; i < world_height; i++) free(obstacles_map[i]);
            free(obstacles_map); obstacles_map = NULL;
        }
        continue;
    }

// --- uvoľnenie pamäte prekážok ---
    if (obstacles_map) {
        for (int i = 0; i < world_height; i++)
            free(obstacles_map[i]);
            free(obstacles_map);
        }
    
    return 0;
    }
}