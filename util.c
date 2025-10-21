#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

void install_handler(int sig, void (*handler)(int)) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = handler;
 
    sigemptyset(&sa.sa_mask);

    sa.sa_flags = SA_RESTART;

    if (sigaction(sig, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}


void SIGINT_handler(int sig) {
    (void)sig;
    return;
}

void SIGQUIT_handler(int sig) {
    (void)sig; 
    exit(0);
}