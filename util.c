#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

void install_handler(int sig, void (*handler)(int)) {
    struct sigaction sa;
    
    /* Clear the sigaction structure */
    memset(&sa, 0, sizeof(sa));
    
    /* Set the handler function */
    sa.sa_handler = handler;
    
    /* Initialize the signal mask to empty */
    sigemptyset(&sa.sa_mask);
    
    /* SA_RESTART: Restart functions if interrupted by handler */
    sa.sa_flags = SA_RESTART;
    
    /* Install the signal handler */
    if (sigaction(sig, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

/**
 * SIGINT_handler - Handle SIGINT signal (Ctrl-C)
 * Used by snake demos to kill a snake
 */
void SIGINT_handler(int sig) {
    (void)sig;  /* Unused parameter */
    /* Just return - let the program continue */
    return;
}

/**
 * SIGQUIT_handler - Handle SIGQUIT signal (Ctrl-\)
 * Used by snake demos to quit the program
 */
void SIGQUIT_handler(int sig) {
    (void)sig;  /* Unused parameter */
    /* Exit the program cleanly */
    exit(0);
}