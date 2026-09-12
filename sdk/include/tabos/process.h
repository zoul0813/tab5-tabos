#ifndef TABOS_PROCESS_H
#define TABOS_PROCESS_H

#include <tabos/wait.h>

#define TABOS_PROCESS_ARG_MAX 16

int tabos_exec(const char* path, int argc, const char* const argv[]);
/* Concurrent child, copied arguments, actual positive PID or negative error.
 * Child receives no foreground console, keyboard or fullscreen display grant. */
int tabos_spawn(const char* path, int argc, const char* const argv[]);
/* Wait and reap one direct child. Returns PID or negative error. status may be NULL. */
int tabos_waitpid(int pid, int* status);
/* Level-triggered READABLE until reaped; foreign/reaped child is invalid. */
tabos_wait_source_t tabos_process_wait_source(int pid);
int execve(const char* path, char* const argv[], char* const envp[]);
int waitpid(int pid, int* status, int options);

#endif
