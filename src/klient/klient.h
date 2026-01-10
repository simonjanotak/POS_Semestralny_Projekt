/* klient.h — verejné deklarácie pre klient.c */
#ifndef KLIENT_H
#define KLIENT_H

/* Zdieľané konštanty (musí súhlasiť s klient.c) */
#define PORT 12345       // port, na ktorý sa klient pripája
#define MAX_BUFFER 1024  // veľkosť bufferu pre prijaté dáta

/* Verejné funkcie poskytované klient.c */
void print_main_menu(void);                                    // vypíše hlavné menu používateľovi
int connect_to_server(void);                                    // vytvorí TCP spojenie na server
int spawn_server(void);                                         // spustí lokálny server (fork + execl)
void calculate_map_size(const char* filename, int* out_width, int* out_height); // určí rozmery mapy z fileu
void process_server(int sock, int world_width, int world_height, int mod);     // spracuje správy zo servera

#endif /* KLIENT_H */
