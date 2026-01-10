#include "world.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* Vytvorenie novej mapy sveta s danou šírkou, výškou a typom (s prekážkami alebo bez) */
World* world_create(int width, int height, WorldType type) {
    World* w = malloc(sizeof(World));
    if (!w) return NULL;

    w->width = width;
    w->height = height;
    w->type = type;

    /* alokácia 2D poľa buniek */
    w->cells = malloc(height * sizeof(int*));
    for (int y = 0; y < height; y++) {
        w->cells[y] = calloc(width, sizeof(int)); // inicializácia na 0
    }

    return w;
}

/* Zničenie sveta a uvoľnenie pamäte */
void world_destroy(World* w) {
    if (!w) return;

    for (int y = 0; y < w->height; y++)
        free(w->cells[y]);

    free(w->cells);
    free(w);
}

/* Overenie, či je dané políčko prekážkou (1 = prekážka, 0 = voľné) */
int world_is_obstacle(World* w, int x, int y) {
    if (!w) return 0;
    /* ak svet nemá prekážky, nič nie je prekážka */
    if (w->type == WORLD_NO_OBSTACLES) return 0;
    /* štart (0,0) je vždy voľný */
    if (x == 0 && y == 0) return 0;
    return (w->cells[y][x] != 0) ? 1 : 0;
}

/* "Zabalenie" súradníc do mapy (toroid) */
void world_wrap(World* w, int *x, int *y) {
    if (*x < 0) *x = w->width - 1;
    if (*x >= w->width) *x = 0;
    if (*y < 0) *y = w->height - 1;
    if (*y >= w->height) *y = 0;
}

/* Načítanie mapy s prekážkami zo súboru: '0' = voľné, '1' = prekážka */
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

        /* AI - GitHub Copilot : podporuje aj číselné hodnoty s možným mínusom */
        if (c == '-' || isdigit(c)) {
            ungetc(c, file);
            int val;
            if (fscanf(file, "%d", &val) == 1) {
                w->cells[y][x] = (val != 0) ? 1 : 0;
                x++;
                if (x >= w->width) { x = 0; y++; }
                continue;
            } else {
                fgetc(file); // preskočenie nesprávneho znaku
            }
        } 
    }

    fclose(file);
}

/* Náhodné vygenerovanie prekážok na mape (okrem štartu 0,0) */
void world_generate_obstacles(World* w, int count) {
    if (w->type != WORLD_WITH_OBSTACLES)
        return;

    int placed = 0;
    while (placed < count) {
        int x = rand() % w->width;
        int y = rand() % w->height;

        if (x == 0 && y == 0) continue; // štartové políčko voľné
        if (w->cells[y][x] == 0) {
            w->cells[y][x] = 1;
            placed++;
        }
    }
}
