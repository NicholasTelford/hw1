#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define FALSE 0
#define TRUE 1
#define INPUT_STRING_SIZE 80

#include "io.h"
#include "parse.h"
#include "process.h"
#include "shell.h"


char str[80];

int cmd_quit(tok_t arg[]) {
  printf("Bye\n");
  exit(0);
  return 1;
}

int cmd_help(tok_t arg[]);

int cmd_cd(tok_t arg[]) {
  char cur_in;
  int i;
  char dir[80] ="";
  printf("%s", "Enter the name of the new directory: ");
  for(i=0; i<=80; i++) {
	cur_in = getchar();
	if(cur_in != '\n')
		dir[i] = cur_in;
	else
		break;
  }
  chdir(dir);
  getcwd(str, 80);
 
}


/* Command Lookup table */
typedef int cmd_fun_t (tok_t args[]); /* cmd functions take token array and return int */
typedef struct fun_desc {
  cmd_fun_t *fun;
  char *cmd;
  char *doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
  {cmd_help, "?", "show this help menu"},
  {cmd_quit, "quit", "quit the command shell"},
  {cmd_cd, "cd", "change the working directory"},
};

int cmd_help(tok_t arg[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    printf("%s - %s\n",cmd_table[i].cmd, cmd_table[i].doc);
  }
  return 1;
}

int lookup(char cmd[]) {
  int i;
  for (i=0; i < (sizeof(cmd_table)/sizeof(fun_desc_t)); i++) {
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0)) return i;
  }
  return -1;
}

void init_shell()
{
  /* Check if we are running interactively */
  shell_terminal = STDIN_FILENO;

  /** Note that we cannot take control of the terminal if the shell
      is not interactive */
  shell_is_interactive = isatty(shell_terminal);

  if(shell_is_interactive){

    /* force into foreground */
    while(tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp()))
      kill( - shell_pgid, SIGTTIN);

    signal(SIGINT, SIG_IGN);
    signal(SIGSTOP, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    shell_pgid = getpid();

    /* Put shell in its own process group */
    if(setpgid(shell_pgid, shell_pgid) < 0){
      perror("Couldn't put the shell in its own process group");
      exit(1);
    }

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);
    tcgetattr(shell_terminal, &shell_tmodes);
  }
  /** YOUR CODE HERE */
}

/**
 * Add a process to our process list
 */
void add_process(process* p)
{
  process *pnow = malloc(sizeof(process));

  p->prev = pnow;
  pnow->next = p;
}


process* create_process(int argc, char *argv[], pid_t pid)
{
  process *p1 = malloc(sizeof(process));
  int status;

  p1->argv = argv;
  p1->argc = argc;
  p1->pid = pid;
  p1->completed = 0;
  p1->stopped = 0;
  p1->background = 0;
  p1->tmodes = shell_tmodes;
  p1->stdin = 0;
  p1->stdout = 1;
  p1->stderr = 2;
  p1->next = NULL;
  p1->prev = NULL;
 
  waitpid(pid, &status, 0);
  p1->completed = 1;
  return p1;
}



int shell (int argc, char *argv[]) {
  char *s = malloc(INPUT_STRING_SIZE+1);			/* user input string */
  tok_t *t;
  int fd;
  int i;
  int j;			/* tokens parsed from input */
  int lineNum = 0;
  int fundex = -1;
  pid_t pid = getpid();		/* get current processes PID */
  pid_t ppid = getppid();	/* get parents PID */
  pid_t cpid, tcpid, cpgid;

  init_shell();

  printf("%s running as PID %d under %d\n",argv[0],pid,ppid);

  
  getcwd(str, 80);
  fprintf(stdout, "%s: ", str);
  while ((s = freadln(stdin))){
    t = getToks(s); /* break the line into tokens */
    i = isDirectTok(t, ">");
    j = isDirectTok(t, "<");
    fundex = lookup(t[0]); /* Is first token a shell literal */
    if(fundex >= 0) cmd_table[fundex].fun(&t[1]);
    else {
	pid_t child;
	char *envar;
	char *env = "test";
	envar = getenv(env);
	process* p1 = create_process(argc, argv, pid);
        add_process(p1);
	setpgid(pid,pid);
	if((child = fork()) == 0) {
		int status;
		pid_t pid = waitpid(child, &status, 0);
		fflush(stdout);
		if(i == 1) {
			fd = open(t[2], O_WRONLY | O_TRUNC | O_CREAT, 0644);
			write(fd, t[0], strlen(t[0]));
			dup2(fd, 1);
			close(fd);
                }
		if(j == 1) {
			fd = open(t[2], O_RDONLY);
			dup2(fd, 0);
			close(fd);
	 }
		p1->pid = getpid();

		execvpe(t[0], t, envar);
		launch_process(p1);
		
		
		fflush(stdout);

		
	}
		
    }
    fprintf(stdout, "%s: ", str);
  }
  return 0;
}
