/* 

   bosh.c : BOSC shell 

 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parser.h"
#include "print.h"

/* --- symbolic constants --- */
#define HOSTNAMEMAX 100
#define READ_END 0
#define WRITE_END 1

/* --- global variables   --- */
pid_t runningProcess;

char *gethostname(char *hostname)
{
  FILE * file = fopen("/proc/sys/kernel/hostname", "r");
  char * status = fgets(hostname, HOSTNAMEMAX, file);
  fclose(file);
  return status;
}

void signalcallback(int signum) {
  if (signum == SIGINT) {
    if (runningProcess) {
      kill(runningProcess, SIGTERM);
    }
  }
}

int executecommand(Cmd * cmd, char * in) {
  char ** args = cmd->cmd;
  // Redirect input, if needed
  char * inArgs[] = {args[0], in, NULL};
  if (execvp(args[0], in ? inArgs : args) < 0) {
    printf("Command '%s' not found.\n", args[0]);
  }
}

void pipecommand(Cmd * cmd, char * in) {
  char ** args = cmd->cmd;
  
  // Examine for recursive pipes
  if (cmd->next == NULL) { // No more piped commands, just execute
    executecommand(cmd, in);
  } else {                 // We have piped commands!
    // Create a pipe
    int fp[2];
    if (pipe(fp) < 0) fprintf(stderr, "Error when creating pipe\n");

    // Forking child process
    pid_t child = fork();
    if (child < 0) { // Error
      fprintf(stderr, "Error when forking.\n");
    } else if (child == 0) { // Child
      //  Close read end (we only write)
      close(fp[READ_END]);
      // Redirect stdout to the parent
      dup2(fp[WRITE_END], 1); 
      // Execute the current command
      pipecommand(cmd->next, NULL);
    } else { // Parent
      // We do not write
      close(fp[WRITE_END]);
      dup2(fp[READ_END], 0);
      close(fp[READ_END]);
      // Continue with the current command
      executecommand(cmd, in);

      // Wait for the child to end
      waitpid(child, NULL, NULL);
    }
  }
}

/* --- execute a shell command --- */
int executeshellcmd (Shellcmd *shellcmd) {
  // Fork the process
  runningProcess = fork();
  if (runningProcess < 0) { // Error
    fprintf(stderr, "Error when forking.\n");
    return 1;
  } else if (runningProcess == 0) { // Child
    // Execute
     char * in  = shellcmd->rd_stdin;
     char * out = shellcmd->rd_stdout;
     Cmd  * cmd = shellcmd->the_cmds; // The initial command

     // Redirect output, if needed
     if (out) { 
       int file = open(out, O_CREAT | O_TRUNC | O_RDWR, 0666);
       if (file < 0) return; // Return; error
       if (dup2(file, 1) < 0) return; // Return; error
     } else if (shellcmd->background) {
       // Redirect standard output if the process is in the background
       dup2(open("/dev/null", O_RDWR), 1);
     }
  
     // Recursively execute the piped shellcommands (if any)
     pipecommand(cmd, in);
  } else { // Parent
    if (shellcmd->background) {
      printf("Started background process with id %i\n", runningProcess);
      signal(SIGCHLD, SIG_IGN); // Avoid defunct processes
    } else {
      // Wait for child process
      // (and that process only)
      waitpid(runningProcess, NULL, NULL); 
    }
    return 0;
  }
}

/* --- main loop of the simple shell --- */
int main(int argc, char* argv[]) {
  // Setup signal handling
  signal(SIGINT, signalcallback);
  
  /* initialize the shell */
  char *cmdline;
  char hostname[HOSTNAMEMAX];
  int terminate = 0;
  Shellcmd shellcmd;
  
  if (gethostname(hostname)) {

    hostname[strlen(hostname) - 1] = 0; // Remove line break

    char * prefix = strcat(hostname, "$: ");

    /* parse commands until exit or ctrl-c */
    while (!terminate) {
      if (cmdline = readline(prefix)) {
	if(*cmdline) {
	  add_history(cmdline);
       	  if (parsecommand(cmdline, &shellcmd)) {
	    // Check for exit-argument
	    if (strcmp((&shellcmd)->the_cmds->cmd[0], "exit") == 0) {
	      printf("\n");
	      return 0;
	    }
	    // Run shell command
	    terminate = executeshellcmd(&shellcmd);
	  }
	}
	free(cmdline);
      } else terminate = 1;
    }
    printf("Exiting bosh.\n");
  }    
    
  return EXIT_SUCCESS;
}

