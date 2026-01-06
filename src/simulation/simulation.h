// simulation.h
#ifndef SIMULATION_H
#define SIMULATION_H

#include "../world/world.h"
#include "../walker/walker.h"

typedef struct {
    World* world;
    int K;              // max krokov
    int replications;   // počet replikácií
    float p_up, p_down, p_left, p_right;
    int interactive;    // 0 = summary, 1 = interactive
    int client_sock;    // soket klienta
    /* stored results after a single run */
    double *sum_steps;       /* length = width*height */
    int *success_counts;     /* length = width*height */
    char **representative_traj; /* per-cell stored trajectory strings */
} Simulation;

Simulation* simulation_create(int width, int height, int type,
                              int K, int replications,
                              float p_up, float p_down, float p_left, float p_right,
                              int interactive, int client_sock);

void simulation_destroy(Simulation* sim);
void simulation_run(Simulation* sim);
/* send stored results to a client socket */
void simulation_send_summary(Simulation *sim, int client_sock);
void simulation_send_interactive(Simulation *sim, int client_sock);

#endif
