/* Example demonstrates use of a pipe that mimics the use of pipes in a shell 
   Takes the name of the file you want to view using a pager program
   like "more" or "less"; Displays the file's contents by: Creating a
   child process that execs the pager. The parent passes the name of
   the file to the child 
   Original Source: Advanced Programming in the Unix Enviroment, by Richard Stevens
                    http://www.apuebook.com/apue3e.html
   Annotated by: Ramesh Yerraballi
   System Calls used:  fork, exec, wait, pipe, dup2 

*/

#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define	MAXLINE	4096			/* max line length */

#define	DEF_PAGER	"/bin/more"		/* default pager program */

int
main(int argc, char *argv[])
{
    int		n;
    int		fd[2];
    pid_t	pid;
    char	*pager, *argv0;
    char	line[MAXLINE];
    FILE	*fp;
    
    if (argc != 2) {
	fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
	exit(EXIT_FAILURE);
    }
    
    if ((fp = fopen(argv[1], "r")) == NULL)
	fprintf(stderr,"can't open %s", argv[1]);
    if (pipe(fd) < 0)
	    fprintf(stderr,"pipe error");
    
    if ((pid = fork()) < 0) {
	fprintf(stderr,"fork error");
    } else if (pid > 0) {								/* parent */
	close(fd[0]);		/* close read end */
	/* parent copies contents of file given in argv[1] to pipe */
	while (fgets(line, MAXLINE, fp) != NULL) {
	    n = strlen(line);
	    if (write(fd[1], line, n) != n)
		fprintf(stderr,"write error to pipe");
	}
	
	if (ferror(fp))
	    fprintf(stderr,"fgets error");
	
	close(fd[1]);	/* close write end of pipe for reader */
	
	if (waitpid(pid, NULL, 0) < 0)
	    fprintf(stderr,"waitpid error");
	exit(0);
    } else {										/* child */
	close(fd[1]);	/* close write end */
	if (fd[0] != STDIN_FILENO) {
	    if (dup2(fd[0], STDIN_FILENO) != STDIN_FILENO)
		fprintf(stderr,"dup2 error to stdin");
	    close(fd[0]);	/* don't need this after dup2 */
	}
	
	/* get arguments for execl() */
	if ((pager = getenv("PAGER")) == NULL)
		pager = DEF_PAGER;
	if ((argv0 = strrchr(pager, '/')) != NULL)
	    argv0++;		/* step past rightmost slash */
	else
	    argv0 = pager;	/* no slash in pager */
	
	if (execl(pager, argv0, (char *)0) < 0)
	    fprintf(stderr,"execl error for %s", pager);
    }
    exit(0);
}
