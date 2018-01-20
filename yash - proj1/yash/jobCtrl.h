#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <stdbool.h>

void jobCtrl(char *command, int fg, int pid);
void jobsP(int i);
void foreground();
void background();
void changeLocation(int pid);
void changeState(int pid);
void reorganize(int pid);
void removeJob(int pid);
