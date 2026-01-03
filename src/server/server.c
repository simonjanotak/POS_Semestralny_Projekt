#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 12345
#define MAX_CLIENTS 5
#define GRID_SIZE 5

// jednoduchá náhodná pochôdzka
int random_walk_steps(int x, int y) {
    int steps = 0;
    while (x != 0 || y != 0) {
        int dir = rand() % 4;
        if (dir == 0 && x > 0) x--;
        else if (dir == 1 && x < GRID_SIZE-1) x++;
        else if (dir == 2 && y > 0) y--;
        else if (dir == 3 && y < GRID_SIZE-1) y++;
        steps++;
    }
    return steps;
}

int main() {
    srand(time(NULL));

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) {
            perror("accept");
            continue;
        }

        printf("Client connected!\n");

        char buffer[1024] = {0};
        int x = rand() % GRID_SIZE;
        int y = rand() % GRID_SIZE;

        int steps = random_walk_steps(x, y);
        snprintf(buffer, sizeof(buffer), "Starting at (%d,%d), steps to (0,0): %d\n", x, y, steps);

        send(new_socket, buffer, strlen(buffer), 0);
        close(new_socket);
        printf("Result sent to client.\n");
    }

    close(server_fd);
    return 0;
}
