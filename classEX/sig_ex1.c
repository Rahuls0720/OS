/* This is a simple example of signals
   The program sets up functions that will be called in response to the two
   signals (SIGINT and SIGTSTP). The actions performed are print and exit or just print
*/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
/*
These are the POSIX signals, their numbers and what they are for
      Signal     Signo     Action   Comment
       ──────────────────────────────────────────────────────────────────────
       SIGHUP        1       Term    Hangup detected on controlling terminal
                                     or death of controlling process
       SIGINT        2       Term    Interrupt from keyboard (Ctrl-C)
       SIGQUIT       3       Core    Quit from keyboard
       SIGILL        4       Core    Illegal Instruction
       SIGABRT       6       Core    Abort signal from abort(3)
       SIGFPE        8       Core    Floating point exception
       SIGKILL       9       Term    Kill signal
       SIGSEGV      11       Core    Invalid memory reference
       SIGPIPE      13       Term    Broken pipe: write to pipe with no
                                     readers
       SIGALRM      14       Term    Timer signal from alarm(2)
       SIGTERM      15       Term    Termination signal
       SIGUSR1   30,10,16    Term    User-defined signal 1
       SIGUSR2   31,12,17    Term    User-defined signal 2
       SIGCHLD   20,17,18    Ign     Child stopped or terminated

       SIGCONT   19,18,25    Cont    Continue if stopped
       SIGSTOP   17,19,23    Stop    Stop process
       SIGTSTP   18,20,24    Stop    Stop typed at terminal (Ctrl-Z)
       SIGTTIN   21,21,26    Stop    Terminal input for background process
       SIGTTOU   22,22,27    Stop    Terminal output for background process

       The signals SIGKILL and SIGSTOP cannot be caught, blocked, or ignored.
To send a signal to a process from the shell:
   prompt>> kill -<Signal or Signo> <process_pid> 
*/

static void sig_int(int signo) {
  printf("caught SIGINT\n");
  exit(0);
}

static void sig_tstp(int signo) {
	printf("caught SIGTSTP\n");
}

static void sig_handler(int signo) {
  switch(signo){
  case SIGINT:
	printf("caught SIGINT\n");
	exit(0);
	break;
  case SIGTSTP:
       	printf("caught SIGTSTP\n");
	break;
  }
	
}



int main(void) {
// You can register separate signal handlers like here:
  /*  if (signal(SIGINT, sig_int) == SIG_ERR)
    printf("signal(SIGINT) error");
  if (signal(SIGCHLD, sig_chld) == SIG_ERR)
  printf("signal(SIGCHLD) error"); */

  // OR you can have a commong signal handler and do the appropriate action
  // based on the signal number
  if (signal(SIGINT, sig_handler) == SIG_ERR)
    printf("signal(SIGINT) error");
  if (signal(SIGTSTP, sig_handler) == SIG_ERR)
    printf("signal(SIGTSTP) error");
  
  while(1){}
}
