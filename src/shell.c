/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include "readcmd.h"
#include "csapp.h"


static void run_pipeline(struct cmdline *l) {
    sigset_t mask, prec;
	sigemptyset(&mask); // init
	sigaddset(&mask,SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &mask, &prec) <0) {
		perror("sigprocmask");
		return;
	}
	
	int n = 0;
    while (l->seq[n] != NULL) {
		n++;
	}
    if (n == 0) return;

    pid_t *pids = malloc(sizeof(pid_t) * n);
    if (!pids) { 
		perror("malloc"); 
		return; 
	}

    int lect_prec = -1;      // bout lecture du pipe preced
    pid_t dern_pid = -1;

    for (int i = 0; i < n; i++) {
        int pfd[2];
		pfd[0]= -1;
		pfd[1]= -1;
		if (i< n - 1) {
            if (pipe(pfd) < 0) {
                perror("pipe");
                if (lect_prec != -1) close(lect_prec); // fermer pioe ancien
                for (int k = 0; k < i; k++) {
					waitpid(pids[k], NULL, 0); // attendre ceux deja lances (eviterzombies) 
				}
				free(pids);
                return;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            if (lect_prec != -1) close(lect_prec);
            if (pfd[0] != -1) close(pfd[0]);
            if (pfd[1] != -1) close(pfd[1]);
            for (int k = 0; k < i; k++) {
				waitpid(pids[k], NULL, 0);
			}
			free(pids);
            return;
        }

        if (pid == 0) {
            if (i > 0) {  // Si pas 1ere cmd, stdin vient du pipe prced
                if (dup2(lect_prec, STDIN_FILENO) < 0) {
                    perror("dup2");
                    _exit(1);
                }
            }
            if (i < n - 1) {
                if (dup2(pfd[1], STDOUT_FILENO) < 0) {             // Si pas derniere cmd, stdout est le pipe courant
                    perror("dup2");
                    _exit(1);
                }
            }

            if (i == 0 && l->in) {  // si 1ere cmd
                int fd_in = open(l->in, O_RDONLY);
                if (fd_in < 0) { 
					perror(l->in);
					exit(1); 
				}
                if (dup2(fd_in, STDIN_FILENO) < 0) { 
					perror("dup2");
					exit(1); 
				}
                close(fd_in);
            }
            if (i == n - 1 && l->out) { // si derniere cmd
                int fd_out = open(l->out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd_out < 0) { 
					perror(l->out); 
					exit(1); 
				}
                if (dup2(fd_out, STDOUT_FILENO) < 0) { 
					perror("dup2"); 
					exit(1); 
				}
                close(fd_out);
            }
            if (lect_prec != -1) close(lect_prec);
            if (pfd[0] != -1) close(pfd[0]);
            if (pfd[1] != -1) close(pfd[1]);

            sigprocmask(SIG_SETMASK, &prec, NULL);
			execvp(l->seq[i][0], l->seq[i]);
            perror("execvp");
			exit(1);

        }

//pere
        pids[i] = pid;
        if (i == n - 1) dern_pid = pid;
        if (lect_prec != -1) close(lect_prec);
        if (pfd[1] != -1) close(pfd[1]);
        lect_prec = pfd[0];
    }

	if (lect_prec != -1) close(lect_prec);

    
	if (! l->bg){ 
		for (int i = 0; i < n; i++) {
        	int status;
        	if (waitpid(pids[i], &status, 0) < 0) {
            	perror("waitpid");
        	}
    	}
	}
    free(pids);
}



static void sigchild_handler (int numsig) {
	(void) numsig; // pour enlever le warning 
	int olderrno=errno;
	int status;
	pid_t pid;

	pid=waitpid(-1, &status, WNOHANG | WUNTRACED);
	while (pid > 0){
	}

	errno = olderrno;
	
} 

static void inst_sigchild_handler () {
	struct sigaction sa;
	sa.sa_handler = sigchild_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags=SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) < 0) {
		perror("sigaction(SIGCHILD)");
		exit(1);
	}
}



int main()
{
	inst_sigchild_handler(); // enlever les zombies

	while (1) {
		struct cmdline *l;
		int i, j;

    	while (waitpid(-1, NULL, WNOHANG) > 0) {	// pour les task du bg
    	}

		printf("shell> ");
		l = readcmd();

		/* If input stream closed, normal termination */
		if (!l) {
			printf("exit\n");
			exit(0);
		}

		if (l->seq[0] == NULL) continue;	

		if (strcmp(l->seq[0][0],"quit")== 0) {
			printf("quitter\n");
			exit(0);
		}
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}
		
		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);

		run_pipeline(l);

/*		pid_t pid;
		pid=fork();
		if (pid < 0) { 
			perror("fork"); 
			continue; 
		}
		if(pid == 0) {
			if (l->seq[0] != NULL && l->seq[1] == NULL) {
				if (l->in) {
					int fd= open(l->in, O_RDONLY);
					if (fd== -1) {
						perror(l->in);
						exit(1);
					}
					if (dup2(fd, STDIN_FILENO) < 0) {
						perror("dup2");
						exit(1);	
					}
					close (fd);
				}
				if (l->out) {
					int fd1 = open(l->out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
					if (fd1 == -1 ) {
						perror(l->out);
						exit(1);
					}
					if(dup2(fd1, STDOUT_FILENO) < 0) {
						perror("dup2");
						exit(1);
					}
					close (fd1);
				}
				
				execvp(l->seq[0][0], l->seq[0]);
				printf("%s : command not found \n", l->seq[0][0]);
				exit(1);
			
				} else if (l->seq[0] != NULL && l->seq[1] != NULL && l->seq[2] == NULL) {
				 	int pfd[2];
				 	if (pipe(pfd) <0) {
						perror("pipe");
						continue;;
					}

				pid_t pid1 = fork();
			    if (pid1 < 0) {
        			perror("fork");
        			close(pfd[0]); close(pfd[1]);
        			continue;
    			}	 	
				if (pid1==0) {
					if (l->in) {
					int fd_in= open(l->in, O_RDONLY);
					if (fd_in== -1) {
						perror(l->in);
						exit(1);
					}
					if (dup2(fd_in, STDIN_FILENO) < 0) {
						perror("dup2");
						exit(1);	
					}
					close (fd_in);
				 	}

					if (dup2(pfd[1], STDOUT_FILENO)< 0) {
						perror("dup2");
						exit(1);
					}

					close(pfd[0]);
					close(pfd[1]);

					execvp(l->seq[0][0], l->seq[0]);
					perror("execvp");
					exit(1);

				}
				
				pid_t pid2= fork();
				if (pid2 < 0) {
					perror("fork");
					close(pfd[0]);close(pfd[1]);
					waitpid(-1,NULL,0); // si le fils 1 a ete cree donc faut l'attendre
					continue;
				}

				if (pid2 == 0) {
					if (l->out) {
						int fd_out = open(l->out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
						if (fd_out == -1 ) {
							perror(l->out);
							exit(1);
						}
						if(dup2(fd_out, STDOUT_FILENO) < 0) {
							perror("dup2");
							exit(1);
						}
						close (fd_out);
					}

					if (dup2(pfd[0], STDIN_FILENO)< 0) {
						perror("dup2");
						exit(1);
					}
					close(pfd[0]);
					close(pfd[1]);
					execvp(l->seq[1][0], l->seq[1]);
					perror("execvp");
					exit(1);

				}

				close(pfd[0]);
				close(pfd[1]);
				if (waitpid(pid1, NULL, 0) < 0) perror("waitpid");
    			if (waitpid(pid2, NULL, 0) < 0) perror("waitpid");
				continue;
			}
			exit(0);
			if (waitpid(pid, NULL, 0) < 0) perror("waitpid");
		

		}
		if (waitpid(pid, NULL, 0) < 0) perror("waitpid"); */
		
		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
			for (j=0; cmd[j]!=0; j++) {
				printf("%s ", cmd[j]);
			}
			printf("\n");
		}
	}
}