
#ifndef SERVER_H
#define SERVER_H

#include <stddef.h>

/* Konštanty servera (musia byť v súlade s implementáciou v server.c) */
#define PORT 12345       /* port, na ktorom server počúva */
#define MAX_CLIENTS 1    /* maximálny počet súčasne pripojených klientov */
#define MAX_BUFFER 1024  /* veľkosť bufferu pre prijaté dáta */

/* NOTE: typedef pre 'Simulation' je definovaný v simulation.h,
   preto ho tu nedefinujeme znova (predchádza to konfliktu). */

int server_run(void);

#endif /* SERVER_H */
// ...existing code...