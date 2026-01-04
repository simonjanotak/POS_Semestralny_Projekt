#ifndef WALKER_H
#define WALKER_H

typedef struct {
    int x;
    int y;
} Walker;

void walker_step(Walker* w, int dir);

#endif
