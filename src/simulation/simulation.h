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
} Simulation;

Simulation* simulation_create(int width, int height, WorldType type,
                              int K, int replications,
                              float p_up, float p_down, float p_left, float p_right,
                              int interactive, int client_sock);

void simulation_destroy(Simulation* sim);
void simulation_run(Simulation* sim);

#endif
