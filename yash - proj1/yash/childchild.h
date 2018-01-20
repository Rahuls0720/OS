#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>

int pipeFunc(char *arr[], int Pindex, int *fileRedir, int fg);
