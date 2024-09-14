/*****************************************************************************/
/*                           CSC209-24s A3 CSCSHELL                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/

#include "cscshell.h"


// COMPLETE
int cd_cscshell(const char *target_dir){
    if (target_dir == NULL) {
        char user_buff[MAX_USER_BUF];
        if (getlogin_r(user_buff, MAX_USER_BUF) != 0) {
           perror("run_command");
           return -1;
        }
        struct passwd *pw_data = getpwnam((char *)user_buff);
        if (pw_data == NULL) {
           perror("run_command");
           return -1;
        }
        target_dir = pw_data->pw_dir;
    }

    if(chdir(target_dir) < 0){
        perror("cd_cscshell");
        return -1;
    }
    return 0;
}


int *execute_line(Command *head){
    #ifdef DEBUG
    printf("\n***********************\n");
    printf("BEGIN: Executing line...\n");
    #endif

    if (head == NULL) {
      return NULL;
    }

    if (strcmp(head->exec_path, CD) == 0) {
	     int *return_value = malloc(sizeof(int));
       if (return_value == NULL) {
         perror("execute_line");
         return (int *) -1;
       }
       *return_value = cd_cscshell(head->args[1]);
   	   return return_value;
    }

    Command *curr = head;
    Command *tail = head->next;
    int num_commands = 0;

    // Count the number of commands
    while (curr != NULL) {
        num_commands++;
	      tail = curr;
        curr = curr->next;
    }

    // Array to store child process IDs
    pid_t pids[num_commands];

    // Set up file descriptors for pipes
    int pipes[2];

    curr = head;
    while (curr->next != NULL) {
      pipe(pipes);
      curr->stdout_fd = pipes[1];
      curr->next->stdin_fd = pipes[0];
      curr = curr->next;
    }

    // Redirect input/output for the first command
    if (head->redir_in_path != NULL) {
        int fd_in = open(head->redir_in_path, O_RDONLY);
        if (fd_in == -1) {
            perror("open");
            ERR_PRINT(ERR_EXECUTE_LINE);
            return (int *) -1;
        }
	      if (head->stdin_fd != STDIN_FILENO) {
	          close(head->stdin_fd);
	      }
        head->stdin_fd = fd_in;
    }

    // Redirect output for the last command
    if (tail->redir_out_path != NULL) {
        int fd_out;
        if (tail->redir_append) {
            fd_out = open(tail->redir_out_path, O_WRONLY | O_CREAT |
                          O_APPEND, 0644);
        }
        else {
            fd_out = open(tail->redir_out_path, O_WRONLY | O_CREAT |
                          O_TRUNC, 0644);
        }
        if (fd_out == -1) {
            perror("open");
            return (int *) -1;
        }
	      if (tail->stdout_fd != STDOUT_FILENO) {
	          close(tail->stdout_fd);
	      }
        tail->stdout_fd = fd_out;
    }

    curr = head;
    int i = 0;
    while (curr != NULL) {
        pids[i] = run_command(curr);
        if (pids[i] == -1) {
          return (int *) -1;
        }
        i++;
        curr = curr->next;
    }

    #ifdef DEBUG
    printf("All children created\n");
    #endif

    int status;
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], &status, 0);
    }

    #ifdef DEBUG
    printf("All children finished\n");
    #endif

    #ifdef DEBUG
    printf("END: Executing line...\n");
    printf("***********************\n\n");
    #endif

    free_command(head);

    if (WIFEXITED(status)) {
        int *ret = malloc(sizeof(int));
        if (ret == NULL) {
          perror("execute_line");
          return (int *) -1;
        }
        *ret = WEXITSTATUS(status);
        return ret;
    }
    return NULL;
}

/*
** Forks a new process and execs the command
** making sure all file descriptors are set up correctly.
**
** Parent process returns -1 on error.
** Any child processes should not return.
*/
int run_command(Command *command){
    #ifdef DEBUG
    printf("Running command: %s\n", command->exec_path);
    printf("Argvs: ");
    if (command->args == NULL){
        printf("NULL\n");
    }
    else if (command->args[0] == NULL){
        printf("Empty\n");
    }
    else {
        for (int i=0; command->args[i] != NULL; i++){
            printf("%d: [%s] ", i+1, command->args[i]);
        }
    }
    printf("\n");
    printf("Redir out: %s\n Redir in: %s\n",
           command->redir_out_path, command->redir_in_path);
    printf("Stdin fd: %d | Stdout fd: %d\n",
           command->stdin_fd, command->stdout_fd);
    #endif

    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        perror("fork");
        return -1;
    }
    else if (pid == 0) {
        // Child process
        // Assign file descriptors using dup2
        if (command->stdin_fd != STDIN_FILENO) {
            if (dup2(command->stdin_fd, STDIN_FILENO) == -1) {
                perror("dup2");
                return -1;
            }
            close(command->stdin_fd);
        }

        if (command->stdout_fd != STDOUT_FILENO) {
            if (dup2(command->stdout_fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                return -1;
            }
            close(command->stdout_fd);
        }

        // Execute the command
        if (execv(command->exec_path, command->args) == -1) {
            perror("execv");
            return -1;
        }
    } else {
        // Parent process
        // Close file descriptors from the command struct
        if (command->stdin_fd != STDIN_FILENO) {
            close(command->stdin_fd);
        }

        if (command->stdout_fd != STDOUT_FILENO) {
            close(command->stdout_fd);
        }

        return pid;
    }

    return 0;

    #ifdef DEBUG
    printf("Parent process created child PID [%d] for %s\n", pid,
            command->exec_path);
    #endif
}


int run_script(char *file_path, Variable **root){
  FILE *file = fopen(file_path, "r");
  if (file == NULL) {
      perror("fopen");
      return -1;
  }

  char line[MAX_SINGLE_LINE];

  while (fgets(line, MAX_SINGLE_LINE, file) != NULL) {
      // Remove newline character if present
      if (line[strlen(line) - 1] == '\n') {
          line[strlen(line) - 1] = '\0';
      }

      Command *commands = parse_line(line, root);
      if (commands == (Command *) -1) {
          fprintf(stderr, "Error parsing line in script: %s\n", line);
          fclose(file);
          return -1;
      }
      if (commands == NULL) continue;

      int *last_ret_code_pt = execute_line(commands);
      if (last_ret_code_pt == (int *) -1) {
          fprintf(stderr, "Error executing line in script: %s\n", line);
          free(last_ret_code_pt);
          fclose(file);
          return -1;
      }
      free(last_ret_code_pt);
  }

  fclose(file);
  return 0;
}

void free_command(Command *command){

  Command *curr_command = command;
  Command *temp_command;

  while (curr_command == NULL) {
    temp_command = curr_command->next;
    int i = 0;
    while (curr_command->args[i] != NULL) {
      free(curr_command->args[i]);
      i++;
    }
    if (curr_command->redir_in_path != NULL) {
      free(curr_command->redir_in_path);
    }
    if (curr_command->redir_out_path != NULL) {
      free(curr_command->redir_out_path);
    }
    free(curr_command->exec_path);
    free(curr_command->args);
    free(curr_command);
    curr_command = temp_command;
  }
}
