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
