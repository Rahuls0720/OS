//child.c - for non pipe commands

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include "jobCtrl.h"

#define Read 0
#define Write 1
#define Maxline 4096


int PID;

static void sig_int (int signo){
	//printf("Sending signals to group:%d\n", PID);
	kill(-PID, SIGINT);
}

static void sig_tstp (int signo){
	//printf("Sending signals to group:%d\n", PID);
	kill(-PID, SIGTSTP);
}

int nonpipeFunc(char *arr[], int *fileRedir, int fg) {
  FILE *fp;
  char buf;
  int status;
  int temp = 0;

  PID = fork();
  tcsetpgrp(STDOUT_FILENO, PID);
  if (PID > 0){ //parent
	  setpgid(PID, PID);
	  //printf("Child1 pid = %d\n", PID);
	 
	  if(signal(SIGINT, sig_int) == SIG_ERR)
		  printf("signal(SIGINT) error");
	  if(signal(SIGTSTP, sig_tstp) == SIG_ERR)
		  printf("signal(SIGTSTP) error");


	  jobCtrl(0, 0, PID);
	  
	  if(fg == 0){
		  signal(SIGTTOU, SIG_IGN);
		  tcsetpgrp(STDOUT_FILENO, getpgrp());
		 
		   temp = waitpid(-PID, &status, WNOHANG);
		   if(temp > 0){
		   if (WIFEXITED(status)) { //if exited successfully or killed (ctrl-c)
			     removeJob(PID);
			     printf("done");
		     }
		   }
	  }else{
		 temp = waitpid(-PID, &status, WUNTRACED);
		     if (WIFEXITED(status) || WIFSIGNALED(status)) { //if exited successfully or killed (ctrl-c)
			     removeJob(PID);
		     }else if (WIFSTOPPED(status)) { //ctrl-z
			     changeState(PID);
			     changeLocation(PID);
		     }
	  }
  }else { // Child
	  if(fileRedir[0] > 0){
		  int count = 0;
		  while(fileRedir[count] != 0 && count < 3){
			 if(strcmp(arr[fileRedir[count]], "<") == 0){ 
				  if((fp = fopen(arr[fileRedir[count] + 1], "r")) != NULL){
					  dup2(fileno(fp), STDIN_FILENO);
				  }
			  }else if(strcmp(arr[fileRedir[count]], "2>") == 0){
				  if((fp = fopen(arr[fileRedir[count] + 1], "w")) != NULL){
					  dup2(fileno(fp), STDERR_FILENO);
				  }
			  }else if(strcmp(arr[fileRedir[count]], ">") ==0){
				   if((fp = fopen(arr[fileRedir[count] + 1], "w")) != NULL){
					   dup2(fileno(fp), STDOUT_FILENO);
				   }
			  }
			 arr[fileRedir[count]] = NULL;
			 count++;
		  }
	  }

	  execvp(arr[0], arr);

	  if(fp != NULL)
		  fclose(fp);
  }
}
