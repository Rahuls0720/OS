/*Either ch1 or parent runs, if parent runs then ch2 created but parent will wait for children to complete and ch2 will wait since nothing has been written to write end of pipe so ch1 ultimately runs first*/

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


int pipefd[2];
int status, pid_ch1, pid_ch2, pid;

static void sig_int(int signo) { //terminates process
 // printf("Sending signals to group:%d\n",pid_ch1); // group id is pid of first in pipeline
 // removeJob(pid_ch1);
  kill(-pid_ch1,SIGINT);
}

static void sig_tstp(int signo) { //stops process
  //printf("Sending SIGTSTP to group:%d\n",pid_ch1); // group id is pid of first in pipeline
  //changeState(pid_ch1);
  kill(-pid_ch1,SIGTSTP);
}

int pipeFunc(char *arr[], int Pindex, int *fileRedir, int fg) {
  FILE *fp;
  int temp;

  if (pipe(pipefd) == -1) { //why pipe first?
    perror("pipe");
    exit(-1);
  }

  pid_ch1 = fork();
  tcsetpgrp(STDOUT_FILENO, pid_ch1);
  if (pid_ch1 > 0){ //parent
	  printf("Child1 pid = %d\n", pid_ch1);
	 
	  pid_ch2 = fork();
	  //tcsetpgrp(STDOUT_FILENO, pid_ch2);
	  if (pid_ch2 > 0){ //parent
	      setpgid(pid_ch2, pid_ch1);
	      printf("Child2 pid = %d\n", pid_ch2);

	      if (signal(SIGINT, sig_int) == SIG_ERR)	
		      printf("signal(SIGINT) error");    
	      if (signal(SIGTSTP, sig_tstp) == SIG_ERR)
		      printf("signal(SIGTSTP) error");
	

	      jobCtrl(0, 0, pid_ch1);
	      
	      close(pipefd[Read]); 
	      close(pipefd[Write]);	       

	 
	      if(fg == 0){
		      signal(SIGTTOU, SIG_IGN);
		      printf("backgrond\n");
		      tcsetpgrp(STDOUT_FILENO, getpgrp());
		      /*waitpid(-1, &status, WUNTRACED);
		      if (WIFEXITED(status) || WIFSIGNALED(status)) { //if exited successfully or killed (ctrl-c)
			      removeJob(pid_ch1);
		      }else if (WIFSTOPPED(status)) { //ctrl-c
			      changeState(pid_ch1);
		      }*/
	      }else{
		      int count = 0;
		      while(count < 1){
			      temp = waitpid(-pid_ch1, &status, 0);
			      if(temp > 0){
				      if (WIFEXITED(status) || WIFSIGNALED(status)) { //if exited successfully or killed (ctrl-c)
					      removeJob(pid_ch1); //exit and write prompt 
					      count++;
				      }else if (WIFSTOPPED(status)) { //ctrl-c
					      changeState(pid_ch1);
				      }
			      }
		      }
	      }
	  }else {//child 2
		  sleep(1);
		  dup2(pipefd[Read], STDIN_FILENO);
		  close(pipefd[Write]);

		  int temp = 0;
		  if(fileRedir[0] > 0){   
			   int count = 0;
			   while(fileRedir[count] != 0 && count < 3 && fileRedir[count] > Pindex){ 
				   if(fileRedir[count] > Pindex){
					 /*  if(strcmp(arr[fileRedir[count]], "<") == 0){
						   if((fp = fopen(arr[fileRedir[count] + 1], "r")) != NULL){
							   dup2(fileno(fp), STDIN_FILENO);
						   }*/
					   if(strcmp(arr[fileRedir[count]], "2>") == 0){
						   if((fp = fopen(arr[fileRedir[count] + 1], "w")) != NULL){
							   dup2(fileno(fp), STDERR_FILENO);
						   }
					   }else if(strcmp(arr[fileRedir[count]], ">") ==0){
						   if((fp = fopen(arr[fileRedir[count] + 1], "w")) != NULL){
							   dup2(fileno(fp), STDOUT_FILENO);
						   }       
					   } 
					   if(temp == 0 && fileRedir[count] > Pindex)
						   temp = fileRedir[count]; //index of first redirection after pipe 
				   }
				   count++;
			   }      
		   }

		  char *myargs[4];	      
		  int count = Pindex + 1;	    
		  int x = 0;
		  if(temp == 0){ 
			  while(arr[count] != NULL){ //no redirection so go to end 
				  myargs[x] = arr[count];	      
				  count++, x++;
			  }
		  }else{
			  while(arr[count] != NULL && count < temp){ //fileRedir[0] is  first file redirection ind
				  myargs[x] = arr[count];	      
				  count++, x++;
			  }
		  }
		  myargs[x] = NULL;
		 
		  
		  if(fp != NULL)
			  fclose(fp);

		  execvp(myargs[0], myargs);  
	  }
  }else {// Child 1
	    setsid(); // child 1 creates a new session and a new group and becomes leader -
		      // group id is same as his pid: pid_ch1
	    dup2(pipefd[Write], STDOUT_FILENO); 
	    close(pipefd[Read]); 
	     
	    if(fileRedir[0] > 0){   
		   int count = 0;
		   while(fileRedir[count] != 0 && count < 3 && fileRedir[count] < Pindex){
			   if(strcmp(arr[fileRedir[count]], "<") == 0){
				   if((fp = fopen(arr[fileRedir[count] + 1], "r")) != NULL){
					   dup2(fileno(fp), STDIN_FILENO);
				   }
			   }else if(strcmp(arr[fileRedir[count]], "2>") == 0){
				   if((fp = fopen(arr[fileRedir[count] + 1], "w")) != NULL){
					   dup2(fileno(fp), STDERR_FILENO);
				   }
			   }/*else if(strcmp(arr[fileRedir[count]], ">") ==0){
				   if((fp = fopen(arr[fileRedir[count] + 1], "w")) != NULL){
					   dup2(fileno(fp), STDOUT_FILENO);
				   }       
			   }*/        
			   count++;
		   }
	    }

	    char *myargs[2];
	    int count = 0;
	    if(fileRedir[0] == 0){ 
		    while(count < Pindex){ 
			    myargs[count] = arr[count];
			    count++;
		    }
	    }else{
		    while(count < Pindex && count < fileRedir[0]){ 
			    myargs[count] = arr[count];
			    count++;
		    }
	    }
	    myargs[count] = NULL;

	    if(fp != NULL)
		  fclose(fp);

	    execvp(myargs[0], myargs);  
  }
}

