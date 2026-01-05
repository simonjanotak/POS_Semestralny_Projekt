#ifndef WALKER_H
#define WALKER_H
#include "../world/world.h"
#include <stdlib.h>

typedef struct {
    int x;
    int y;
} Walker;

void walker_step(Walker* w, World* wolrd,float up, float down, float left, float right);
void walker_init(Walker* w, int start_x, int start_y);
#endif
