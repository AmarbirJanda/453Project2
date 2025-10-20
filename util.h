#ifndef UTIL_H
#define UTIL_H

#include <signal.h>

void install_handler(int sig, void (*handler)(int));

void SIGINT_handler(int sig);
void SIGQUIT_handler(int sig);

#endif