#include "world.h"
#include <stdlib.h>

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
    if (w->type == WORLD_NO_OBSTACLES)
        return 0;

    return w->cells[y][x];
}
void world_wrap(World* w, int *x, int *y) {
    if (*x < 0) *x = w->width - 1;
    if (*x >= w->width) *x = 0;
    if (*y < 0) *y = w->height - 1;
    if (*y >= w->height) *y = 0;
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
