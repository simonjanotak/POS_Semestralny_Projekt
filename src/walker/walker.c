#include "walker.h"

void walker_init(Walker* w, int start_x, int start_y) {
    w->x = start_x;
    w->y = start_y;
}

void walker_step(Walker* w, World* world,
                 float p_up, float p_down,
                 float p_left, float p_right) {

    float r = (float)rand() / RAND_MAX;
    int dir = 0;

    if (r < p_up)          dir = 0; // hore
    else if (r < p_up + p_down) dir = 1; // dole
    else if (r < p_up + p_down + p_left) dir = 2; // vľavo
    else                   dir = 3; // vpravo

    int nx = w->x;
    int ny = w->y;

    if (dir == 0) nx--;
    else if (dir == 1) nx++;
    else if (dir == 2) ny--;
    else if (dir == 3) ny++;

    world_wrap(world, &nx, &ny);

    if (!world_is_obstacle(world, nx, ny)) {
        w->x = nx;
        w->y = ny;
    }
}

