#include <stdio.h>
#include "draw.h"

void draw_world(int width, int height, int **obstacles, int px, int py) {
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            if (x == px && y == py)
                printf(" @ ");
            else if (x == 0 && y == 0)
                printf(" S ");
            else if (obstacles && obstacles[y][x])
                printf(" X ");
            else
                printf(" . ");
        }
        printf("\n");
    }
}

void draw_values(double **values, int width, int height) {
    if (!values) return;
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            double v = values[y][x];
            if (v < 0.0) {
                printf("  -  ");
            } else if (v <= 1.0) {
                // probability
                printf("%5.2f", v);
            } else {
                // average steps
                printf("%5.1f", v);
            }
        }
        printf("\n");
    }
}
