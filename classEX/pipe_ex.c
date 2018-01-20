/* Example demonstrates a simple use of a Pipe to communicate between a parent and a child 
   A pipe is a uni-directional communication mechanism.
   Here the parent writes a string to the child, which the child prints to the screen
   Original Source: Advanced Programming in the Unix Enviroment, by Richard Stevens
                    http://www.apuebook.com/apue3e.html
   Annotated by: Ramesh Yerraballi
   System Calls used:  fork, wait, pipe 
*/
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int
main(int argc, char *argv[])
{
    int pipefd[2];
    pid_t cpid;
    char buf;
    
    if (argc != 2) {
	fprintf(stderr, "Usage: %s <string>\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    // pipe takes a pointer to a 2-element array (pipefd[2]) as input and returns the
    // the read and write end of the pipe in pipefd[0] and write end in pipefd[1]
    if (pipe(pipefd) == -1) {
	perror("pipe");
	exit(EXIT_FAILURE);
    }
    
    cpid = fork();
    if (cpid == -1) {
	perror("fork");
	exit(EXIT_FAILURE);
    }
    
    if (cpid == 0) {    /* Child reads from pipe */
	close(pipefd[1]);          /* Closes unused write end */
	
	while (read(pipefd[0], &buf, 1) > 0)
	    write(STDOUT_FILENO, &buf, 1);
	
	write(STDOUT_FILENO, "\n", 1);
	close(pipefd[0]);
	_exit(EXIT_SUCCESS);

    } else {            /* Parent writes argv[1] to pipe */
	close(pipefd[0]);          /* Close unused read end */
	write(pipefd[1], argv[1], strlen(argv[1]));
        close(pipefd[1]);          /* Reader will see EOF */
        wait(NULL);                /* Wait for child */
        exit(EXIT_SUCCESS);
    }
}
