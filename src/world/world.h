#ifndef WORLD_H
#define WORLD_H

typedef enum {
    WORLD_NO_OBSTACLES,
    WORLD_WITH_OBSTACLES
} WorldType;

typedef struct {
    int width;
    int height;
    WorldType type;
    int **cells;   // 0 = free, 1 = obstacle
} World;

/* vytvorenie / zrušenie */
World* world_create(int width, int height, WorldType type);
void world_destroy(World* w);

/* práca so svetom */
int world_is_obstacle(World* w, int x, int y);

/* wrap-around pohyb */
void world_wrap(World* w, int *x, int *y);

/* generovanie prekážok */
void world_generate_obstacles(World* w, int count);

#endif
