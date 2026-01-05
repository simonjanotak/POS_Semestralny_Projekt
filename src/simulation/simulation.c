// simulation.c
#include "simulation.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

Simulation* simulation_create(int width, int height, int type,
                              int K, int replications,
                              float p_up, float p_down, float p_left, float p_right,
                              int interactive, int client_sock) {
    Simulation* sim = malloc(sizeof(Simulation));
    if (!sim) return NULL;
    WorldType typa = (type == 0) ? WORLD_NO_OBSTACLES : WORLD_WITH_OBSTACLES;
    sim->world = world_create(width, height, typa);
                              
    sim->K = K;
    sim->replications = replications;
    sim->p_up = p_up;
    sim->p_down = p_down;
    sim->p_left = p_left;
    sim->p_right = p_right;
    sim->interactive = interactive;
    sim->client_sock = client_sock;

    if (typa == WORLD_WITH_OBSTACLES) {
        int count = width * height / 5; // napr. 20% políčok
        world_generate_obstacles(sim->world, count);

        // Poslať klientovi súradnice prekážok
        char buf[256];
        for (int y = 0; y < sim->world->height; y++) {
            for (int x = 0; x < sim->world->width; x++) {
                if (world_is_obstacle(sim->world, x, y)) {
                    snprintf(buf, sizeof(buf), "OBSTACLE %d %d\n", x, y);
                    send(sim->client_sock, buf, strlen(buf), 0);
                }
            }
        }
        send(sim->client_sock, "OBSTACLE_END\n", strlen("OBSTACLE_END\n"), 0);
    }

    return sim;
}

void simulation_destroy(Simulation* sim) {
    if (!sim) return;
    world_destroy(sim->world);
    free(sim);
}
void simulation_run(Simulation* sim) {
    char buf[256];
    int w = sim->world->width;
    int h = sim->world->height;

    if (!sim->interactive) {
        double *sum_steps = calloc(w * h, sizeof(double)); // sum of steps for successful runs
        int *success_count = calloc(w * h, sizeof(int));   // number of successful runs

        for (int rep = 1; rep <= sim->replications; rep++) {
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    if (world_is_obstacle(sim->world, x, y)) continue;
                    if(x == 0 && y == 0) continue;
                    Walker wk;
                    walker_init(&wk, x, y);

                    int steps = 0;
                    int hit_center = 0;

                    for (steps = 0; steps < sim->K; steps++) {
                        walker_step(&wk, sim->world,
                                    sim->p_up, sim->p_down,
                                    sim->p_left, sim->p_right);
                        if (wk.x == 0 && wk.y == 0) {
                            hit_center = 1;
                            break;
                        }
                    }

                    if (hit_center) {
                        int idx = y * w + x;
                        success_count[idx] += 1;
                        sum_steps[idx] += (double)steps;
                    }
                }
            }
        }

        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                if (world_is_obstacle(sim->world, x, y)) {
                    snprintf(buf, sizeof(buf), "OBSTACLE %d %d\n", x, y);
                    send(sim->client_sock, buf, strlen(buf), 0);
                    continue;
                }

                int idx = y * w + x;
                double avg_steps = -1.0;
                double prob = 0.0;
                if (success_count[idx] > 0) avg_steps = sum_steps[idx] / success_count[idx];
                prob = (double)success_count[idx] / (double)sim->replications;

                snprintf(buf, sizeof(buf), "SUMMARY %d %d %.3f %.3f\n", x, y, avg_steps, prob);
                send(sim->client_sock, buf, strlen(buf), 0);
            }
        }

        send(sim->client_sock, "SUMMARY_DONE\n", strlen("SUMMARY_DONE\n"), 0);

        free(sum_steps);
        free(success_count);
        return;
    }

    for (int rep = 1; rep <= sim->replications; rep++) {
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {

                if (world_is_obstacle(sim->world, x, y)) continue;

                Walker wkr;
                walker_init(&wkr, x, y);

                int steps = 0;
                int hit_center = 0;

                // START 
                snprintf(buf, sizeof(buf), "START %d %d %d\n", x, y, rep);
                send(sim->client_sock, buf, strlen(buf), 0);

                for (steps = 0; steps < sim->K; steps++) {
                    walker_step(&wkr, sim->world,
                                sim->p_up, sim->p_down,
                                sim->p_left, sim->p_right);

                    snprintf(buf, sizeof(buf), "POS %d %d\n", wkr.x, wkr.y);
                    send(sim->client_sock, buf, strlen(buf), 0);
                    if (wkr.x == 0 && wkr.y == 0) {
                        hit_center = 1;
                        break;
                    }
                }

                // END 
                snprintf(buf, sizeof(buf), "END %d %d\n", steps, hit_center);
                send(sim->client_sock, buf, strlen(buf), 0);
            }
        }
    }
}

