/* file: sig_ex2.c
   Author: Ramesh Yerraballi
   This is a more complex example of signals
   Create a child process and communicate with it using a pipe
   The parent writes to the pipe and child reads from the pipe.
   What the parent writes is what the user is typing on the keyboard
   when the child read two identical characters in a row
   it sends a SIGUSR1 signal to the parent. The parent acknowleges 
   the signal and quits
*/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>


static void sig_handler(int signo) {
  printf("doh\n");
  exit(0);
	
}

int main(void) {

  int pipefd[2], status;
  char ch[2]={0,0},pch=128;
  
  if (pipe(pipefd) == -1) {
    perror("pipe");
    exit(-1);
  }
  
  int ret = fork();
  if (ret > 0){
    // Parent
    if (signal(SIGUSR1, sig_handler) == SIG_ERR)
    printf("signal(SIGUSR1) error");
    close(pipefd[0]); //close read
    while (read(STDIN_FILENO,ch,1) != 0){
      write(pipefd[1],ch,1);
    }
    wait(&status);
    
  }else {
    // Child
    close(pipefd[1]); // close the write end
    while (read(pipefd[0],ch,1) != 0){
      printf("%c",ch[0]);sync();
      if (pch == ch[0]){
	//printf("sending SIGUSR1 to parent\n");
	kill(getppid(),SIGUSR1);
	exit(0);
      }
      pch = ch[0];
    }
    
  }

}
