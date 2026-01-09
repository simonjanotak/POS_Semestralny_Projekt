#ifndef WALKER_H
#define WALKER_H
#include "../world/world.h"
#include <stdlib.h>

typedef struct {
    int x;
    int y;
} Walker;

/* Perform a single move attempt. Returns 1 if the walker moved, 0 if it stayed. */
int walker_step(Walker* w, World* world, float p_up, float p_down, float p_left, float p_right);
void walker_init(Walker* w, int start_x, int start_y);
#endif
