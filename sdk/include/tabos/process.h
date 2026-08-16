#ifndef TABOS_PROCESS_H
#define TABOS_PROCESS_H

#define TABOS_PROCESS_ARG_MAX 16

int tabos_exec(const char *path, int argc, const char *const argv[]);
int tabos_spawn(const char *path, int argc, const char *const argv[]);
int tabos_waitpid(int pid, int *status);
int execve(const char *path, char *const argv[], char *const envp[]);
int waitpid(int pid, int *status, int options);

#endif
