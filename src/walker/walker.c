#include "walker.h"
#include <stdlib.h>
#include <stdbool.h>

void walker_init(Walker* w, int start_x, int start_y) {
    w->x = start_x;
    w->y = start_y;
}



//zmena kroku chodca s ohľadom na prekážky a pravdepodobnosti
int walker_step(Walker* w, World* world,
                 float p_up, float p_down,
                 float p_left, float p_right) {

    /* Sample a move direction without replacement according to provided probabilities.
       If the chosen target is an obstacle, try other directions until a free cell
       is found or all directions are exhausted. */
    float probs[4] = { p_up, p_down, p_left, p_right };
    int dirs[4] = {0,1,2,3};
    bool tried[4] = {false,false,false,false};

    for (int attempt = 0; attempt < 4; ++attempt) {
        /* compute sum of remaining weights */
        float sum = 0.0f;
        for (int i = 0; i < 4; ++i) if (!tried[i]) sum += probs[i];
        if (sum <= 0.0f) break; /* no remaining moves with positive weight */

        float r = ((float)rand() / RAND_MAX) * sum;
        int chosen = -1;
        float acc = 0.0f;
        for (int i = 0; i < 4; ++i) {
            if (tried[i]) continue;
            acc += probs[i];
            if (r <= acc) { chosen = i; break; }
        }
        if (chosen == -1) {
            /* fallback to first available */
            for (int i = 0; i < 4; ++i) if (!tried[i]) { chosen = i; break; }
        }

        /* compute candidate position */
        int nx = w->x;
        int ny = w->y;
        if (chosen == 0) ny--;       /* up */
        else if (chosen == 1) ny++;  /* down */
        else if (chosen == 2) nx--;  /* left */
        else if (chosen == 3) nx++;  /* right */

        world_wrap(world, &nx, &ny);

        if (!world_is_obstacle(world, nx, ny)) {
            w->x = nx;
            w->y = ny;
            return 1;
        }

        /* mark this direction as tried and loop to pick another */
        tried[chosen] = true;
    }
    /* no valid move found (all neighbours blocked or zero-weight) -> stay in place */
    return 0;
}

