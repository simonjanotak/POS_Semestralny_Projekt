#include "simulation.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <pthread.h>
#include <stdatomic.h>

/* =========================
   Pomocné interné funkcie
   ========================= */

/* Nápad AI Chat gpt - Bezpečná verzia strdup, ktorá alokuje a skopíruje string */
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    size_t l = strlen(s) + 1;
    char *d = malloc(l);
    if (!d) return NULL;
    memcpy(d, s, l);
    return d;
}

/* AI Chat GPT - Thread-safe send: posielanie dát klientovi.
   Ak send zlyhá, označí sim->client_sock = -1, aby sa prestalo posielať */
static ssize_t safe_send(Simulation *sim, const char *buf, size_t len) {
    if (!sim) return -1;
    ssize_t res = -1;
    pthread_mutex_lock(&sim->send_mutex); // zámok kvôli viacerým vláknam
    if (sim->client_sock >= 0) {
        res = send(sim->client_sock, buf, len, 0);
        if (res <= 0) {
            /* klient sa pravdepodobne odpojil; socket označíme neplatným */
            sim->client_sock = -1;
        }
    }
    pthread_mutex_unlock(&sim->send_mutex);
    return res;
}

/* Pošli jednu riadok sumarizácie výsledkov */
static int send_summary_line(Simulation *sim, int x, int y, double average_steps, double probability) {
    char message[128];
    snprintf(message, sizeof(message), "SUMMARY %d %d %.3f %.3f\n", x, y, average_steps, probability * 100);
    return safe_send(sim, message, strlen(message)) > 0 ? 0 : -1;
}

/* Ak je políčko prekážkou, pošli ho ako summary */
static void send_obstacle_as_summary(Simulation *sim, int x, int y) {
    send_summary_line(sim, x, y, -1.0, 0.0);
}

/* =========================
   Vytvorenie a zničenie simulácie
   ========================= */

/* Vytvorí štruktúru simulácie a alokuje všetky potrebné polia */
Simulation* simulation_create(int width, int height, int world_type,
                              int max_steps, int replications,
                              float prob_up, float prob_down, float prob_left, float prob_right,
                              int interactive_flag, int client_socket) {
    Simulation* sim = malloc(sizeof(Simulation));
    if (!sim) return NULL;
    memset(sim, 0, sizeof(*sim));

    WorldType wt = (world_type == 0) ? WORLD_NO_OBSTACLES : WORLD_WITH_OBSTACLES;
    sim->world = world_create(width, height, wt);
    if (!sim->world) { free(sim); return NULL; }

    sim->K = max_steps;
    sim->replications = replications > 0 ? replications : 1;
    sim->p_up = prob_up;
    sim->p_down = prob_down;
    sim->p_left = prob_left;
    sim->p_right = prob_right;
    sim->interactive = interactive_flag ? 1 : 0;
    sim->client_sock = client_socket;

    atomic_init(&sim->stop_requested, 0);
    pthread_mutex_init(&sim->send_mutex, NULL);

    int cells = width * height;
    sim->sum_steps = calloc(cells, sizeof(double));
    sim->success_counts = calloc(cells, sizeof(int));
    /* alokuj úložisko pre trajektórie: pre každé políčko a každú replikáciu samostatný slot */
    sim->representative_traj = calloc(cells * sim->replications, sizeof(char*));
    if (!sim->sum_steps || !sim->success_counts || !sim->representative_traj) {
        if (sim->sum_steps) free(sim->sum_steps);
        if (sim->success_counts) free(sim->success_counts);
        if (sim->representative_traj) free(sim->representative_traj);
        world_destroy(sim->world);
        free(sim);
        return NULL;
    }

    return sim;
}

/* Zrušenie simulácie a uvoľnenie všetkých alokovaných polí */
void simulation_destroy(Simulation* sim) {
    if (!sim) return;

    if (sim->representative_traj) {
        int cells = sim->world ? sim->world->width * sim->world->height : 0;
        int total = cells * (sim->replications > 0 ? sim->replications : 1);
        for (int i = 0; i < total; ++i)
            if (sim->representative_traj[i]) free(sim->representative_traj[i]);
        free(sim->representative_traj);
    }

    if (sim->sum_steps) free(sim->sum_steps);
    if (sim->success_counts) free(sim->success_counts);
    if (sim->world) world_destroy(sim->world);
    pthread_mutex_destroy(&sim->send_mutex);
    free(sim);
}

/* =========================
   Vlákna pre výpočet jednej bunky
   ========================= */

/* Argumenty vlákna pre spracovanie jedného štartovacieho políčka */
typedef struct {
    Simulation *sim;
    int start_x;
    int start_y;
    int index;              /* index = y * width + x */
    int max_steps;
    int replications;
    pthread_mutex_t *rand_mutex; /* chráni globálne RNG */
    pthread_mutex_t *send_mutex; /* chráni send() */
} CellWorkerArg;

/* Funkcia vlákna: vykoná všetky replikácie z jedného štartu a uloží výsledky */
static void *cell_worker(void *arg_void) {
    CellWorkerArg *arg = (CellWorkerArg*)arg_void;
    Simulation *sim = arg->sim;
    int start_x = arg->start_x;
    int start_y = arg->start_y;
    int K = arg->max_steps;
    int reps = arg->replications;
    int successes = 0;
    double steps_sum = 0.0;
    int idx = arg->index;

    for (int r = 0; r < reps; r++) {
        /* ak bol požiadaný stop, skonči replikácie */
        if (atomic_load(&sim->stop_requested)) break;

        Walker walker;
        walker_init(&walker, start_x, start_y);
        int hit_center = 0;
        int steps_taken = 0;

        /* buffer pre trajektóriu */
        char traj_buf[8192];
        traj_buf[0] = '\0';

        /* START správa pre interaktívny mód */
        if (sim->interactive && sim->client_sock >= 0) {
            char startmsg[128];
            snprintf(startmsg, sizeof(startmsg), "START %d %d %d\n", start_x, start_y, r+1);
            safe_send(sim, startmsg, strlen(startmsg));
        }

        /* simulácia krokov walkera */
        while (steps_taken < K) {
            if (atomic_load(&sim->stop_requested)) break;

            if (arg->rand_mutex) pthread_mutex_lock(arg->rand_mutex);
            int moved = walker_step(&walker, sim->world, sim->p_up, sim->p_down, sim->p_left, sim->p_right);
            if (arg->rand_mutex) pthread_mutex_unlock(arg->rand_mutex);

            if (moved) {
                steps_taken++;
                char piece[64];
                snprintf(piece, sizeof(piece), "%d %d\n", walker.x, walker.y);
                strncat(traj_buf, piece, sizeof(traj_buf) - strlen(traj_buf) - 1);

                /* interaktívny mód – pošli pozíciu */
                if (sim->interactive && sim->client_sock >= 0) {
                    char posmsg[64];
                    snprintf(posmsg, sizeof(posmsg), "POS %d %d\n", walker.x, walker.y);
                    safe_send(sim, posmsg, strlen(posmsg));
                }

                /* ak dosiahol stred, ukonči */
                if (walker.x == 0 && walker.y == 0) { hit_center = 1; break; }
            } else {
                /* walker sa nepohol – skontroluj voľné susedné bunky */
                int any_free = 0;
                int tx, ty;

                tx = walker.x; ty = walker.y - 1; world_wrap(sim->world, &tx, &ty); if (!world_is_obstacle(sim->world, tx, ty)) any_free = 1;
                tx = walker.x; ty = walker.y + 1; world_wrap(sim->world, &tx, &ty); if (!world_is_obstacle(sim->world, tx, ty)) any_free = 1;
                tx = walker.x - 1; ty = walker.y; world_wrap(sim->world, &tx, &ty); if (!world_is_obstacle(sim->world, tx, ty)) any_free = 1;
                tx = walker.x + 1; ty = walker.y; world_wrap(sim->world, &tx, &ty); if (!world_is_obstacle(sim->world, tx, ty)) any_free = 1;

                if (!any_free) steps_taken++; /* uzavretý walker – spotrebuje krok */
                else continue; /* voľné susedné bunky – retry bez kroku */
            }
        }

        /* update úspechov a sumy krokov */
        if (hit_center) {
            successes++;
            steps_sum += (double)steps_taken;
        }

        /* END správa pre interaktívny mód */
        if (sim->interactive && sim->client_sock >= 0) {
            char endmsg[64];
            snprintf(endmsg, sizeof(endmsg), "END %d %d\n", steps_taken, hit_center);
            safe_send(sim, endmsg, strlen(endmsg));
        }

        /*  AI Copilot - uloženie trajektórie do representative_traj */
        int slot = idx * reps + r;
        if (traj_buf[0] != '\0') sim->representative_traj[slot] = safe_strdup(traj_buf);
        else sim->representative_traj[slot] = safe_strdup(""); /* prázdna trajektória */
    }

    sim->success_counts[idx] = successes;
    sim->sum_steps[idx] = steps_sum;
    return NULL;
}

/* =========================
   Spustenie celej simulácie
   ========================= */

/* Vytvorí vlákno pre každú bunku a spracuje všetky replikácie */
void simulation_run(Simulation* sim) {
    if (!sim || !sim->world) return;

    int width = sim->world->width;
    int height = sim->world->height;
    int cell_count = width * height;
    int reps = sim->replications;

    pthread_mutex_t rand_mutex;
    pthread_mutex_init(&rand_mutex, NULL);

    pthread_t *threads = calloc(cell_count, sizeof(pthread_t));
    CellWorkerArg *args = calloc(cell_count, sizeof(CellWorkerArg));
    if (!threads || !args) {
        free(threads); free(args);
        pthread_mutex_destroy(&rand_mutex);
        return;
    }

    int thread_count = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (world_is_obstacle(sim->world, x, y)) continue;

            if (x == 0 && y == 0) {
                /* stredové políčko – úspech pre všetky replikácie */
                sim->success_counts[idx] = sim->replications;
                sim->sum_steps[idx] = 0.0;
                for (int r = 0; r < reps; ++r) {
                    int slot = idx * reps + r;
                    sim->representative_traj[slot] = safe_strdup("0 0\n");
                }
                continue;
            }

            args[thread_count].sim = sim;
            args[thread_count].start_x = x;
            args[thread_count].start_y = y;
            args[thread_count].index = idx;
            args[thread_count].max_steps = sim->K;
            args[thread_count].replications = reps;
            args[thread_count].rand_mutex = &rand_mutex;
            args[thread_count].send_mutex = &sim->send_mutex;

            if (pthread_create(&threads[thread_count], NULL, cell_worker, &args[thread_count]) != 0) {
                /*AI nápad -  ak vlákno nevznikne, vykonaj úlohu v hlavnom vlákne  */
                cell_worker(&args[thread_count]);
            }
            thread_count++;
        }
    }

    /* počkaj na dokončenie všetkých vlákien */
    for (int i = 0; i < thread_count; ++i) pthread_join(threads[i], NULL);

    free(threads);
    free(args);
    pthread_mutex_destroy(&rand_mutex);
}

/* =========================
   Odoslanie interaktívnych trajektórií
   ========================= */

/*AI - GitHub Copilot Posielanie trajektórie pre jednu replikáciu */
static void send_representative_trajectory_rep(Simulation *sim, int start_x, int start_y, const char *traj_str, int replication_id) {
    char buf[128];
    snprintf(buf, sizeof(buf), "START %d %d %d\n", start_x, start_y, replication_id);
    safe_send(sim, buf, strlen(buf));

    if (!traj_str || traj_str[0] == '\0') {
        /* žiadny pohyb */
        snprintf(buf, sizeof(buf), "END %d %d\n", 0, 0);
        safe_send(sim, buf, strlen(buf));
        return;
    }

    const char *p = traj_str;
    int last_x = start_x, last_y = start_y;
    int steps = 0;
    while (*p) {
        int nx, ny;
        int consumed = 0;
        if (sscanf(p, "%d %d%n", &nx, &ny, &consumed) == 2 && consumed > 0) {
            snprintf(buf, sizeof(buf), "POS %d %d\n", nx, ny);
            safe_send(sim, buf, strlen(buf));
            last_x = nx; last_y = ny;
            steps++;
            p += consumed;
            while (*p == '\r' || *p == '\n') ++p;
        } else break;
    }
    int hit_center = (last_x == 0 && last_y == 0) ? 1 : 0;
    snprintf(buf, sizeof(buf), "END %d %d\n", steps, hit_center);
    safe_send(sim, buf, strlen(buf));
}

/* AI - GitHub Copilot Odoslanie interaktívnych dát všetkých replikácií */
void simulation_send_interactive(Simulation *sim) {
    if (!sim || !sim->world) return;
    int width = sim->world->width;
    int height = sim->world->height;
    int reps = sim->replications;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (world_is_obstacle(sim->world, x, y)) continue;
            for (int r = 0; r < reps; ++r) {
                int slot = idx * reps + r;
                const char *traj = sim->representative_traj[slot];
                send_representative_trajectory_rep(sim, x, y, traj, r+1);
            }
        }
    }
}

/* AI - GitHub Copilot Odoslanie sumarizácie výsledkov (bez interaktívneho streamovania) */
void simulation_send_summary(Simulation *sim) {
    if (!sim || !sim->world) return;
    int width = sim->world->width;
    int height = sim->world->height;
    int reps = sim->replications;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int idx = y * width + x;
            if (world_is_obstacle(sim->world, x, y)) {
                send_obstacle_as_summary(sim, x, y);
                continue;
            }
            if (x == 0 && y == 0) {
                send_summary_line(sim, x, y, 0.0, 1.0);
                continue;
            }
            double avg = -1.0;
            if (sim->success_counts[idx] > 0) avg = sim->sum_steps[idx] / (double)sim->success_counts[idx];
            double prob = (double)sim->success_counts[idx] / (double)reps;
            send_summary_line(sim, x, y, avg, prob);
        }
    }
}
