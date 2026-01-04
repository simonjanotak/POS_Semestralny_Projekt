#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

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

int main() {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[MAX_BUFFER];

    // vytvorenie socketu
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("Socket creation error"); return 1; }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    while (1) {
        print_main_menu();
        int choice;
        scanf("%d", &choice);
        getchar(); // odstráni enter po scanf

        if (choice == 1) { // NOVÁ SIMULÁCIA
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
            filename[strcspn(filename, "\n")] = 0; // odstráni newline

            snprintf(buffer, sizeof(buffer),
                     "NEW_SIM %d %d %d %d %d %d %.2f %.2f %.2f %.2f %s\n",
                     width, height, K, replications, obstacles, mode,
                     p_up, p_down, p_left, p_right, filename);
            send(sock, buffer, strlen(buffer), 0);
            printf("New simulation parameters sent to server.\n");
        }
        else if (choice == 2) { // PRIPOJIŤ SA
            send(sock, "JOIN_SIM\n", 9, 0);
            printf("Joining existing simulation...\n");
        }
        else if (choice == 3) { // OPÄTOVNÉ SPUSTENIE
            char filename[128];
            printf("Enter saved simulation file: ");
            getchar();
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = 0;

            snprintf(buffer, sizeof(buffer), "REPLAY_SIM %s\n", filename);
            send(sock, buffer, strlen(buffer), 0);
            printf("Replay request sent to server.\n");
        }
        else if (choice == 4) { // KONIEC
            send(sock, "QUIT\n", 5, 0);
            break;
        }
        else {
            printf("Invalid choice\n");
            continue;
        }

        // --- Prijímanie odpovedí zo servera ---
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int n = read(sock, buffer, sizeof(buffer)-1);
            if (n <= 0) {
                printf("Server disconnected.\n");
                break;
            }

            printf("%s\n", buffer);
            // tu sa môže doplniť kreslenie interaktívnej mapy podľa "POS x y"
        }
    }

    close(sock);
    return 0;
}
