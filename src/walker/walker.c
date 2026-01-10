#include "walker.h"
#include <stdlib.h>
#include <stdbool.h>

/* -------------------------
   Inicializácia chodca
   -------------------------
   Nastaví počiatočné súradnice chodca
*/
void walker_init(Walker* w, int start_x, int start_y) {
    w->x = start_x;
    w->y = start_y;
}

/* -------------------------
    Čiastočne upravy od AI GitHub Copilot
   Vykoná jeden krok chodca
   - zohľadňuje prekážky a pravdepodobnosti pohybu
   - vráti 1, ak sa chodcovi podarilo pohnúť
   - vráti 0, ak všetky smery blokované (zostane na mieste)
   ------------------------- */
int walker_step(Walker* w, World* world,
                 float p_up, float p_down,
                 float p_left, float p_right) {

    /* uložíme pravdepodobnosti a indexy smerov: 0=up,1=down,2=left,3=right */
    float probs[4] = { p_up, p_down, p_left, p_right };
    int dirs[4] = {0,1,2,3};
    bool tried[4] = {false,false,false,false}; // ktoré smery sme už skúšali

    for (int attempt = 0; attempt < 4; ++attempt) {
        /* spočítame súčet ešte nevyskúšaných váh */
        float sum = 0.0f;
        for (int i = 0; i < 4; ++i)
            if (!tried[i]) sum += probs[i];
        if (sum <= 0.0f) break; /* žiadny dostupný smer s kladnou pravdepodobnosťou */

        /* náhodný výber smeru podľa pravdepodobnosti */
        float r = ((float)rand() / RAND_MAX) * sum;
        int chosen = -1;
        float acc = 0.0f;
        for (int i = 0; i < 4; ++i) {
            if (tried[i]) continue; // tento smer už bol skúšaný
            acc += probs[i];
            if (r <= acc) { chosen = i; break; } // vybraný smer
        }
        if (chosen == -1) {
            /* fallback: zober prvý dostupný smer */
            for (int i = 0; i < 4; ++i) 
                if (!tried[i]) { chosen = i; break; }
        }

        /* vypočítame kandidátsku pozíciu */
        int nx = w->x;
        int ny = w->y;
        if (chosen == 0) ny--;       /* hore */
        else if (chosen == 1) ny++;  /* dole */
        else if (chosen == 2) nx--;  /* vľavo */
        else if (chosen == 3) nx++;  /* vpravo */

        world_wrap(world, &nx, &ny); // ošetrenie hraníc sveta (wrap-around)

        /* ak cieľová bunka nie je prekážka, chodca premiestnime */
        if (!world_is_obstacle(world, nx, ny)) {
            w->x = nx;
            w->y = ny;
            return 1; // pohyb úspešný
        }

        /* ak bunka je prekážka, označíme tento smer ako skúšaný a pokračujeme */
        tried[chosen] = true;
    }

    /* žiadny dostupný pohyb: všetky susedné bunky blokované -> zostane na mieste */
    return 0;
}
