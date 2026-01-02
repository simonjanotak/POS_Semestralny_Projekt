#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    printf("Random Walk Server starting...\n");

    for (int i = 1; i <= 5; i++) {
        printf("Server running... step %d\n", i);
        sleep(1);
    }

    printf("Server shutting down.\n");
    return 0;
}
