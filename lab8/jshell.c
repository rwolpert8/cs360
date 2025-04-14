/* Lab 8: Jshell
   Name: Ryan Wolpert
   Student ID: 000499029
   Description: This program creates a primitive shell.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "dllist.h"
#include "fields.h"
#include "jrb.h"
#include "jval.h"

// Struct to hold command information - recommended from lab writeup
typedef struct {
    char *infile;
    char *outfile;
    int append_out;
    int wait_for_children;
    int n_commands;
    int *argcs;
    char ***argvs;
    Dllist comlist; 
} Command;

// Custom function to count the number of commands in the list
int count_dllist(Dllist l) {
    int count = 0;
    Dllist tmp;
    dll_traverse(tmp, l) {
        count++;
    }
    return count;
}
  
// Free all data in the command structure to avoid leaks
void free_command(Command *cmd) {
    int i, j;
    if (cmd->infile) free(cmd->infile);
    if (cmd->outfile) free(cmd->outfile);
    // Free each command's argv
    for (i = 0; i < cmd->n_commands; i++) {
      for (j = 0; j < cmd->argcs[i]; j++) {
        free(cmd->argvs[i][j]);
      }
      free(cmd->argvs[i]);
    }
    free(cmd->argvs);
    free(cmd->argcs);
    free_dllist(cmd->comlist);

    // Reset fields
    cmd->infile = NULL;
    cmd->outfile = NULL;
    cmd->append_out = 0;
    cmd->wait_for_children = 1;
    cmd->n_commands = 0;
}
  
// This function forks and executes the commands, handling pipes and redirection
void execute_commands(Command *cmd) {
    int i;
    int prev_read_end = -1;
    JRB pid_tree = make_jrb();
  
    for (i = 0; i < cmd->n_commands; i++) {
      fflush(stdin); fflush(stdout); fflush(stderr);
  
      // Create pipe if not the last command
      int pipefd[2];
      if (i < cmd->n_commands - 1) {
        if (pipe(pipefd) < 0) {
          perror("pipe");
        }
      }
  
      pid_t pid = fork();
      if (pid < 0) {
        perror("fork");
      } else if (pid == 0) {
        // Child
        if (i == 0 && cmd->infile) {
          int fd = open(cmd->infile, O_RDONLY);
          if (fd < 0) { 
            perror("open infile"); exit(1); 
          }
          dup2(fd, 0);
          close(fd);
        } else if (prev_read_end != -1) {
          dup2(prev_read_end, 0);
          close(prev_read_end);
        }
  
        // Redirect output if last command or create a pipe
        if (i == cmd->n_commands - 1 && cmd->outfile) {
          int flags = O_WRONLY | O_CREAT;
          flags |= (cmd->append_out ? O_APPEND : O_TRUNC);
          int fd = open(cmd->outfile, flags, 0666);
          if (fd < 0) { 
            perror("open outfile"); exit(1); 
          }
          dup2(fd, 1);
          close(fd);
        } else if (i < cmd->n_commands - 1) {
          dup2(pipefd[1], 1);
          close(pipefd[0]);
          close(pipefd[1]);
        }
  
        execvp(cmd->argvs[i][0], cmd->argvs[i]);
        perror(cmd->argvs[i][0]);
        exit(1);
      } else {
        // Parent
        if (cmd->wait_for_children) jrb_insert_int(pid_tree, pid, new_jval_i(1));
        if (prev_read_end != -1) close(prev_read_end);
        if (i < cmd->n_commands - 1) {
          close(pipefd[1]);
          prev_read_end = pipefd[0];
        }
      }
    }
  
    // If we must wait, wait for all children
    if (cmd->wait_for_children) {
      while (!jrb_empty(pid_tree)) {
        int status;
        pid_t done = wait(&status);
        if (jrb_find_int(pid_tree, done)) {
          jrb_delete_node(jrb_find_int(pid_tree, done));
        }
      }
      jrb_free_tree(pid_tree);
    }
}
  
// Read lines, build Command, handle END, etc
int main(int argc, char **argv) {
    Command cmd;
    memset(&cmd, 0, sizeof(Command));
    cmd.wait_for_children = 1;
    cmd.comlist = new_dllist();
  
    IS is = new_inputstruct(NULL);
    while (get_line(is) >= 0) {
      if (is->NF == 0 || is->fields[0][0] == '#') continue;
  
      if (!strcmp(is->fields[0], "NOWAIT")) {
        cmd.wait_for_children = 0;
      }
      else if (!strcmp(is->fields[0], "<")) {
        if (is->NF > 1) cmd.infile = strdup(is->fields[1]);
      }
      else if (!strcmp(is->fields[0], ">")) {
        if (is->NF > 1) cmd.outfile = strdup(is->fields[1]);
      }
      else if (!strcmp(is->fields[0], ">>")) {
        if (is->NF > 1) {
          cmd.outfile = strdup(is->fields[1]);
          cmd.append_out = 1;
        }
      }
      else if (!strcmp(is->fields[0], "END")) {
        // Build argcs/argvs from comlist
        cmd.n_commands = count_dllist(cmd.comlist);
        cmd.argcs = malloc(sizeof(int) * cmd.n_commands);
        cmd.argvs = malloc(sizeof(char **) * cmd.n_commands);
  
        // Move from dllist to arrays
        int i = 0;
        Dllist tmp;
        dll_traverse(tmp, cmd.comlist) {
          char **this_argv = (char **) jval_v(tmp->val);
          // count how many fields
          int k = 0;
          while (this_argv[k] != NULL) k++;
          cmd.argcs[i] = k;
          cmd.argvs[i] = this_argv;
          i++;
        }
  
        // Execute
        execute_commands(&cmd);
  
        // Free
        free_command(&cmd);

        memset(&cmd, 0, sizeof(Command));
        cmd.wait_for_children = 1;
        cmd.comlist = new_dllist();
      } 
      else {
        // Store as argv in comlist
        char **myargv = malloc(sizeof(char*)*(is->NF+1));
        for (int j = 0; j < is->NF; j++) {
          myargv[j] = strdup(is->fields[j]);
        }
        myargv[is->NF] = NULL;
        dll_append(cmd.comlist, new_jval_v((void*)myargv));
      }
    }
    jettison_inputstruct(is);
    return 0;
}