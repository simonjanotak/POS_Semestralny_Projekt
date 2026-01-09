#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

World* world_create(int width, int height, WorldType type) {
    World* w = malloc(sizeof(World));
    if (!w) return NULL;

    w->width = width;
    w->height = height;
    w->type = type;

    w->cells = malloc(height * sizeof(int*));
    for (int y = 0; y < height; y++) {
        w->cells[y] = calloc(width, sizeof(int));
    }

    return w;
}

void world_destroy(World* w) {
    if (!w) return;

    for (int y = 0; y < w->height; y++)
        free(w->cells[y]);

    free(w->cells);
    free(w);
}

int world_is_obstacle(World* w, int x, int y) {
    if (!w) return 0;
    /* ak svet bez prekážok -> nikdy prekážka */
    if (w->type == WORLD_NO_OBSTACLES) return 0;
    /* zabezpečiť, že štart (0,0) je vždy voľný */
    if (x == 0 && y == 0) return 0;
    return (w->cells[y][x] != 0) ? 1 : 0;
}
void world_wrap(World* w, int *x, int *y) {
    if (*x < 0) *x = w->width - 1;
    if (*x >= w->width) *x = 0;
    if (*y < 0) *y = w->height - 1;
    if (*y >= w->height) *y = 0;
}
void read_file_with_obstacles(World* w, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) return;

    int x = 0, y = 0;
    int c;
    while ((c = fgetc(file)) != EOF && y < w->height) {
        if (c == '0' || c == '1') {
            w->cells[y][x] = (c == '1') ? 1 : 0;
            x++;
            if (x >= w->width) { x = 0; y++; }
            continue;
        }

        if (c == '-' || isdigit(c)) {
            ungetc(c, file);
            int val;
            if (fscanf(file, "%d", &val) == 1) {
                w->cells[y][x] = (val != 0) ? 1 : 0;
                x++;
                if (x >= w->width) { x = 0; y++; }
                continue;
            } else {
                fgetc(file);
            }
        } 
    }

    fclose(file);
}
void world_generate_obstacles(World* w, int count) {
    if (w->type != WORLD_WITH_OBSTACLES)
        return;

    int placed = 0;
    while (placed < count) {
        int x = rand() % w->width;
        int y = rand() % w->height;

        if (x == 0 && y == 0) continue;
        if (w->cells[y][x] == 0) {
            w->cells[y][x] = 1;
            placed++;
        }
    }
}
