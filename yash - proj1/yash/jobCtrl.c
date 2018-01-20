#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>

#define MaxProcess 5

typedef struct {
	bool location; //true means fg
	bool state; //true means running
	char process[200]; 
	int gpid; //group pid of process!
}jobCtrlStruct;

static jobCtrlStruct jobCtrlArr[MaxProcess]; 
static int jobs = 0;

void jobCtrl(char *command, int fg, int pid){
	if(command == 0){ //called when process id is esstablished
		jobCtrlArr[jobs - 1].gpid = pid;
	}else{
		if(fg == 1)
			jobCtrlArr[jobs].location = true; //in fg
		else
			jobCtrlArr[jobs].location = false;
		jobCtrlArr[jobs].state = true; //umm
		jobCtrlArr[jobs].gpid = pid;
		strcpy(jobCtrlArr[jobs].process, command);
		jobs++;
	}
}

void jobsP(int i){ //prints the jobs list if i == 0, else print specific job
	if(i == 0){
		while(i < jobs){
			printf("[%i] ", i + 1); //fix
			if(jobCtrlArr[i].location == true)
				printf("+ ");
			else
				printf("- ");
			if(jobCtrlArr[i].state == true)
				printf("Running ");
			else
				printf("Stopped ");
			printf("        %s\n", jobCtrlArr[i].process);

			i++;
		}
	}else{	
		printf("[%i] ", i); //i doesnt start at 0 in this instance
		if(jobCtrlArr[i].location == true)
			printf("+ ");
		else
			printf("- ");
		if(jobCtrlArr[i].state == true)
			printf("Running ");
		else
			printf("Stopped ");
		printf("        %s\n", jobCtrlArr[i].process);
	}
}



void reorganize(int index){
	int i = 0;
	jobCtrlStruct temp[MaxProcess]; //temp for reeorganization

	while(i < index){
		temp[i] = jobCtrlArr[i];
		i++;
	}

	i++;
	while(i < jobs){
		temp[i] = jobCtrlArr[i];
		i++;
	}

	i = 0;
	while(i < jobs){
		jobCtrlArr[i] = temp[i];
		i++;
	}
	
	jobs = jobs - 1; //update number of jobs
}

void removeJob(int pid){
	int i = 0;
	while(i < jobs){
		if(pid == jobCtrlArr[i].gpid){
			reorganize(i);
			break;
		}
		i++;
	}
}
void changeLocation(int pid){
	int i = 0;
	while(i < jobs){
		if(pid == jobCtrlArr[i].gpid){
			jobCtrlArr[i].location = !(jobCtrlArr[i].location); //not running
			break;
		}
		i++;
	}
}

void changeState(int pid){
	int i = 0;
	while(i < jobs){
		if(pid == jobCtrlArr[i].gpid){
			jobCtrlArr[i].state = !(jobCtrlArr[i].state); //not running
			break;
		}
		i++;
	}
}

void foreground(){
	int i = jobs - 1;
	while(i >= 0){ //signal to most recent bg or stopped process
		if(!(jobCtrlArr[i].location)){ //puts most recent stopped/bg process in fg and cont
			printf("%s\n", jobCtrlArr[i].process);
			changeLocation(jobCtrlArr[i].gpid);
			changeState(jobCtrlArr[i].gpid);
			tcsetpgrp(STDOUT_FILENO, jobCtrlArr[i].gpid);
			kill(-jobCtrlArr[i].gpid, SIGCONT);//wiat? 
			break; 
		}
		i--;
	}
}

void background(){ //run in background
	int i = jobs - 1;
	while(i >= 0){ //signal to most recent stopped process
		if(!(jobCtrlArr[i].location) && !(jobCtrlArr[i].location)){ //in bg and not running
			jobsP(i); 
			changeState(jobCtrlArr[i].gpid);
			signal(SIGTTOU, SIG_IGN);
			tcsetpgrp(STDOUT_FILENO, getpgrp()); //give control to shell!
			kill(-jobCtrlArr[i].gpid, SIGCONT);
			break; //wait?
		}
		i--;
	}
}
