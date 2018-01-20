//Project 1: Yash Shell - Yet Another Sh

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include "childchild.h"
#include "child.h"
#include "jobCtrl.h"

#define Read 0
#define Write 1
#define MaxInputLength 2000

int fg;

//parses out input string
char** parser(char *command, char *arr[]){
	int count = 0;

	char *temp = (char*)malloc(2000 * sizeof(char));
	strcpy(temp, command); //for printing process in jobCtrl()
	
	arr[count] = strtok (command, " "); //todo: add "this can be a file name" input type functionality 
	while (arr[count] != NULL) { //todo: add back slash character functionality //does it even work? 
		//printf("%s ", arr[count]);
		count++;
		arr[count] = strtok(NULL, " ");
	}

	if(strcmp(arr[count - 1], "&") == 0){
		//background process
		fg = 0;
		arr[count - 1] = NULL;
	}else{
		fg = 1;
		arr[count] = NULL;
	}

	if(strcmp(arr[0], "jobs") != 0){
		jobCtrl(temp, fg, 0);
	}

	return arr;
}

//exec inputted command
void execCommand(char* arr[], int *fileRedir){ 
	int count = 0;
	int frIndex = 0;
	int pipeIndex = 0;

	if(strcmp(arr[count], "fg") == 0)
		foreground();
	else if(strcmp(arr[count], "bg") == 0)
		background();
	else if(strcmp(arr[count], "jobs") == 0){
		jobsP(0);
	}else{
		while(arr[count] != NULL){
			if(strcmp(arr[count], ">") == 0 || strcmp(arr[count], ">>") == 0 || strcmp(arr[count], "<") == 0 || strcmp(arr[count], "2>") == 0){
				fileRedir[frIndex] = count;
				frIndex++;
			}
			if(strcmp(arr[count], "|") == 0) 
				pipeIndex = count;
			count++;
		}
		if(pipeIndex > 0)
			pipeFunc(arr, pipeIndex, fileRedir, fg);
		else
			nonpipeFunc(arr, fileRedir, fg);
	}
}

int main(int argc, char *argv[]){	
	int inputSize = 4; 
	int stringLength = 10;
	char* command = (char*)malloc(MaxInputLength * sizeof(char));

	while(1){
		//check if done with pre command
		signal(SIGTTOU, SIG_IGN);
		tcsetpgrp(STDOUT_FILENO, getpgrp());
		printf("# ");
		if((fgets(command, MaxInputLength, stdin)) == NULL){ //exit on EOF
			printf("\n");
			exit(0);
		}
		command[strlen(command) - 1] = 0; //truncate \n

		//allocate arr
		char** arr = (char**)malloc(sizeof(char*) * inputSize);
		for(int i = 0; i < inputSize; i++)
			arr[i] = malloc(stringLength);
		parser(command, arr);

		int *fileRedir = (int*)malloc(3 * sizeof(int)); //array of 3 elements*/
		execCommand(arr, fileRedir);

		//free arr
		/*free(command);
		  free(fileRedir);
		for(int i = 0; i < inputSize; i++)
			free(arr[i]);
		free(arr);*/	
	}
	return 0;
}
