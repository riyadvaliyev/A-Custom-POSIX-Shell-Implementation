/*****************************************************************************/
/*                           CSC209-24s A3 CSCSHELL                          */
/*       Copyright 2024 -- Demetres Kostas PhD (aka Darlene Heliokinde)      */
/*****************************************************************************/

#include "cscshell.h"

#define CONTINUE_SEARCH NULL


// COMPLETE
char *resolve_executable(const char *command_name, Variable *path){

    if (command_name == NULL || path == NULL){
        return NULL;
    }

    if (strcmp(command_name, CD) == 0){
        return strdup(CD);
    }

    if (strcmp(path->name, PATH_VAR_NAME) != 0){
        ERR_PRINT(ERR_NOT_PATH);
        return NULL;
    }

    char *exec_path = NULL;

    if (strchr(command_name, '/')){
        exec_path = strdup(command_name);
        if (exec_path == NULL){
            perror("resolve_executable");
            return NULL;
        }
        return exec_path;
    }

    // we create a duplicate so that we can mess it up with strtok
    char *path_to_toke = strdup(path->value);
    if (path_to_toke == NULL){
        perror("resolve_executable");
        return NULL;
    }
    char *current_path = strtok(path_to_toke, ":");

    do {
        DIR *dir = opendir(current_path);
        if (dir == NULL){
            ERR_PRINT(ERR_BAD_PATH, current_path);
            //closedir(dir);
            continue;
        }

        struct dirent *possible_file;

        while (exec_path == NULL) {
            // rare case where we should do this -- see: man readdir
            errno = 0;
            possible_file = readdir(dir);
            if (possible_file == NULL) {
                if (errno > 0){
                    perror("resolve_executable");
                    closedir(dir);
                    goto res_ex_cleanup;
                }
                // end of files, break
                break;
            }

            if (strcmp(possible_file->d_name, command_name) == 0){
                // +1 null term, +1 possible missing '/'
                size_t buflen = strlen(current_path) +
                    strlen(command_name) + 1 + 1;
                exec_path = (char *) malloc(buflen);
                // also sets remaining buf to 0
                strncpy(exec_path, current_path, buflen);
                if (current_path[strlen(current_path)-1] != '/'){
                    strncat(exec_path, "/", 2);
                }
                strncat(exec_path, command_name, strlen(command_name)+1);
            }
        }
        closedir(dir);

        // if this isn't null, stop checking paths
        if (possible_file) break;

    } while ((current_path = strtok(CONTINUE_SEARCH, ":")));

res_ex_cleanup:
    free(path_to_toke);
    return exec_path;
}

// Helper function to remove the leading whitespace from line
char *clear_leading_whitespace(char *line) {

    while (isspace((unsigned char)*line)) {  // Remove leading whitespace
      line++;
    }

    if(line[0] == '\0') { // 0 is ASCII # for NULL (check if the line is empty)
      return line;
    }

    return line;
}

// Helper function to remove the trailing whitespace from line
char *clear_trailing_whitespace(char *line) {

    char *end = line + strlen(line) - 1;

    while(end >= line && isspace((unsigned char)*end)) { // Remove trailing w/s
      end--;
    }

    end[1] = '\0'; // Mark the end of line

    return line;
}

// Parse an individual command (seperated by pipes)
Command *parse_a_command(char *line, Variable *path, bool *prev_pipe_exists,
  bool *output_exists) {

    line = clear_leading_whitespace(line);

    Command *command = malloc(sizeof(Command));
    if (command == NULL) {
      perror("parse_a_command");
      return (Command *) -1;
    }

    // Check allocation
    command->args = NULL;
    command->next = NULL;
    command->stdin_fd = STDIN_FILENO;
    command->stdout_fd = STDOUT_FILENO;
    command->redir_in_path = NULL;
    command->redir_out_path = NULL;
    command->redir_append = 0;

    char* input_redir = strchr(line, '<');
    // Raise an error b/c we already have an input from the previous pipe
    if (input_redir != NULL && (*prev_pipe_exists)) {
      free(command);
      ERR_PRINT(ERR_PARSING_LINE);
      return NULL;
    }
    char* output_redir = strchr(line, '>');
    char* output_a_redir = strstr(line, ">>");

    // Check various combinations of redirections
    if (input_redir != NULL && output_redir == NULL && output_a_redir == NULL) {

      (*input_redir) = '\0';
      input_redir = clear_leading_whitespace(input_redir + 1);
      input_redir = clear_trailing_whitespace(input_redir);

      if (strpbrk(input_redir, " \t\n") != NULL || strlen(input_redir) == 0) {
        free(command);
        ERR_PRINT(ERR_PARSING_LINE);
        return NULL;
      }

      command->redir_in_path = strdup(input_redir);
      if (command->redir_in_path == NULL) {
        free(command);
        perror("parse_a_command");
        return (Command *) -1;
      }

    } else
    if (input_redir == NULL && output_redir != NULL && output_a_redir == NULL) {
      // We have an output redirection here, so setting *output_exists = true
      // and next pipe won't run
      (*output_exists) = true;

      (*output_redir) = '\0';
      output_redir = clear_leading_whitespace(output_redir + 1);
      output_redir = clear_trailing_whitespace(output_redir);

      if (strpbrk(output_redir, " \t\n") != NULL || strlen(output_redir) == 0) {
        free(command);
        ERR_PRINT(ERR_PARSING_LINE);
        return NULL;
      }

      command->redir_out_path = strdup(output_redir);
      if (command->redir_out_path == NULL) {
        free(command);
        perror("parse_a_command");
        return (Command *) -1;
      }

    } else
    if (input_redir == NULL && output_redir != NULL && output_a_redir != NULL) {
      // We have an output redirection here, so setting *output_exists = true
      // and next pipe won't run
      (*output_exists) = true;

      output_a_redir[0] = '\0';
      output_a_redir[1] = '\0';
      output_a_redir = clear_leading_whitespace(output_a_redir + 2);
      output_a_redir = clear_trailing_whitespace(output_a_redir);

      if (strpbrk(output_a_redir, " \t\n") != NULL ||
          strlen(output_a_redir) == 0) {
        free(command);
        ERR_PRINT(ERR_PARSING_LINE);
        return NULL;
      }

      command->redir_out_path = strdup(output_a_redir);
      if (command->redir_out_path == NULL) {
        free(command);
        perror("parse_a_command");
        return (Command *) -1;
      }
      command->redir_append = NON_ZERO_BYTE;

    } else
    if (input_redir != NULL && output_redir != NULL && output_a_redir == NULL) {
      // We have an output redirection here, so setting *output_exists = true
      // and next pipe won't run
      (*output_exists) = true;

      if (input_redir > output_redir) { // cmd (.arg.) > output.txt < input.txt

        // STEP 1: First, handle input redirection (since it comes after)
        (*input_redir) = '\0';
        input_redir = clear_leading_whitespace(input_redir + 1);
        input_redir = clear_trailing_whitespace(input_redir);

        if (strpbrk(input_redir, " \t\n") != NULL || strlen(input_redir) == 0) {
          // raise an error saying that there are arguments after redirected
          // files, which's illegal or no filename was provided after rediretion
          free(command);
          ERR_PRINT(ERR_PARSING_LINE);
          return NULL;
        }

        command->redir_in_path = strdup(input_redir);
        if (command->redir_in_path == NULL) {
          free(command);
          perror("parse_a_command");
          return (Command *) -1;
        }
        // STEP 2: Now, handle output redirection
        (*output_redir) = '\0';
        output_redir = clear_leading_whitespace(output_redir + 1);
        output_redir = clear_trailing_whitespace(output_redir);

        if (strpbrk(output_redir, " \t\n") != NULL
                   || strlen(output_redir) == 0) {
          free(command->redir_in_path);
          free(command);
          ERR_PRINT(ERR_PARSING_LINE);
          return NULL;
        }

        command->redir_out_path = strdup(output_redir);
        if (command->redir_out_path == NULL) {
          free(command->redir_in_path);
          free(command);
          perror("parse_a_command");
          return (Command *) -1;
        }

      } else { // cmd (..arg..) < input.txt > output.txt

        // STEP 1: First, handle output redirection (since it comes after)
        (*output_redir) = '\0';
        output_redir = clear_leading_whitespace(output_redir + 1);
        output_redir = clear_trailing_whitespace(output_redir);

        if (strpbrk(output_redir, " \t\n") != NULL
                   || strlen(output_redir) == 0) {
          free(command);
          ERR_PRINT(ERR_PARSING_LINE);
          return NULL;
        }

        command->redir_out_path = strdup(output_redir);
        if (command->redir_out_path == NULL) {
          free(command);
          perror("parse_a_command");
          return (Command *) -1;
        }

        // STEP 2: Now, handle input redirection
        (*input_redir) = '\0';
        input_redir = clear_leading_whitespace(input_redir + 1);
        input_redir = clear_trailing_whitespace(input_redir);

        if (strpbrk(input_redir, " \t\n") != NULL || strlen(input_redir) == 0) {
          free(command->redir_out_path);
          free(command);
          ERR_PRINT(ERR_PARSING_LINE);
          return NULL;
        }

        command->redir_in_path = strdup(input_redir);
        if (command->redir_in_path == NULL) {
          free(command->redir_out_path);
          free(command);
          perror("parse_a_command");
          return (Command *) -1;
        }

      }
    } else
    if (input_redir != NULL && output_redir != NULL && output_a_redir != NULL) {
        (*output_exists) = true;

        if (input_redir > output_a_redir) { // cmd >> output.txt < input.txt

          // STEP 1: First, handle input redirection (since it comes after)
          (*input_redir) = '\0';
          input_redir = clear_leading_whitespace(input_redir + 1);
          input_redir = clear_trailing_whitespace(input_redir);

          if (strpbrk(input_redir, " \t\n") != NULL ||
              strlen(input_redir) == 0) {
            free(command);
            ERR_PRINT(ERR_PARSING_LINE);
            return NULL;
          }

          command->redir_in_path = strdup(input_redir);
          if (command->redir_in_path == NULL) {
            free(command);
            perror("parse_a_command");
            return (Command *) -1;
          }

          // STEP 2: Now, handle output append redirection
          output_a_redir[0] = '\0';
          output_a_redir[1] = '\0';
          output_a_redir = clear_leading_whitespace(output_a_redir + 2);
          output_a_redir = clear_trailing_whitespace(output_a_redir);

          if (strpbrk(output_a_redir, " \t\n") != NULL ||
              strlen(output_a_redir) == 0) {
            free(command->redir_in_path);
            free(command);
            ERR_PRINT(ERR_PARSING_LINE);
            return NULL;
          }

          command->redir_out_path = strdup(output_a_redir);
          if (command->redir_out_path == NULL) {
            free(command->redir_in_path);
            free(command);
            return NULL;
          }

          command->redir_append = NON_ZERO_BYTE;

        } else { // cmd (..arg..) < input.txt >> output.txt

          // STEP 1: First, handle output append redirection b/c it comes after
          output_a_redir[0] = '\0';
          output_a_redir[1] = '\0';
          output_a_redir = clear_leading_whitespace(output_a_redir + 2);
          output_a_redir = clear_trailing_whitespace(output_a_redir);

          if (strpbrk(output_a_redir, " \t\n") != NULL ||
              strlen(output_a_redir) == 0) {
            free(command);
            ERR_PRINT(ERR_PARSING_LINE);
            return NULL;
          }

          command->redir_out_path = strdup(output_a_redir);
          if (command->redir_out_path == NULL) {
            free(command);
            perror("parse_a_command");
            return (Command *) -1;
          }
          command->redir_append = NON_ZERO_BYTE;

          // STEP 2: Now, handle input redirection
          (*input_redir) = '\0';
          input_redir = clear_leading_whitespace(input_redir + 1);
          input_redir = clear_trailing_whitespace(input_redir);

          if (strpbrk(input_redir, " \t\n") != NULL ||
              strlen(input_redir) == 0) {
            free(command->redir_out_path);
            free(command);
            ERR_PRINT(ERR_PARSING_LINE);
            return NULL;
          }

          input_redir = strdup(command->redir_in_path);
          if (input_redir == NULL) {
            free(command->redir_out_path);
            free(command);
            perror("parse_a_command");
            return (Command *) -1;
          }
        }

    } else
    if (input_redir == NULL && output_redir == NULL && output_a_redir == NULL) {
      // do nothing
    } else {
      // raise error b/c it's an invalid combination of redirections
      free(command);
      ERR_PRINT(ERR_PARSING_LINE);
      return NULL;
    }

    // Count the number of arguments
    int arg_count = 0;

    for (int i = 0; i < strlen(line); i++) {
      if (isspace(line[i]) && (!isspace(line[i+1]) && (line[i+1] != '\0')) ) {
        arg_count++;
      }
    }

    // + 2: one for exec_path at command->args[0], the other for NULL at the end
    command->args = malloc((arg_count + 2) * sizeof(char*));
    if (command->args == NULL) {
      if (command->redir_in_path != NULL) {
        free(command->redir_in_path);
      }
      if (command->redir_out_path != NULL) {
        free(command->redir_out_path);
      }
      free(command);
      perror("parse_a_command");
      return (Command *) -1;
    }

    char *token_for_command;
    // The first word is always command_name
    char *command_name = strtok_r(line, " \t\n", &token_for_command);
    char *path_to_executable = resolve_executable(command_name, path);
    if (path_to_executable == NULL) {
      if (command->redir_in_path != NULL) {
        free(command->redir_in_path);
      }
      if (command->redir_out_path != NULL) {
        free(command->redir_out_path);
      }
      free(command->args);
      free(command);
      perror("parse_a_command");
      return (Command *) -1;
    }

    command->exec_path = strdup(path_to_executable);
    if (command->exec_path == NULL) {
      if (command->redir_in_path != NULL) {
        free(command->redir_in_path);
      }
      if (command->redir_out_path != NULL) {
        free(command->redir_out_path);
      }
      free(command->args);
      free(command);
      perror("parse_a_command");
      return (Command *) -1;
    }

    command->args[0] = strdup(path_to_executable);
    if (command->args[0] == NULL) {
      if (command->redir_in_path != NULL) {
        free(command->redir_in_path);
      }
      if (command->redir_out_path != NULL) {
        free(command->redir_out_path);
      }
      free(command->exec_path);
      free(command->args);
      free(command);
      perror("parse_a_command");
      return (Command *) -1;
    }

    // One by one, store arguments in args struct member
    char* token;
    for (int i = 1; i < (arg_count + 1); i++) {
      token = strtok_r(NULL, " \t\n", &token_for_command);
      command->args[i] = strdup(token);
      if (command->args[i] == NULL) {
          for (int j = 0; j < i; j++) {
            free(command->args[j]);
          }
          if (command->redir_in_path != NULL) {
            free(command->redir_in_path);
          }
          if (command->redir_out_path != NULL) {
            free(command->redir_out_path);
          }
          free(command->exec_path);
          free(command->args);
          free(command);
          perror("parse_a_command");
          return (Command *) -1;
      }
    }

    // Making the last argument NULL so we can detect the end during freeing
    command->args[arg_count + 1] = NULL;

    // Making this true to indicate that this pipe exists in the next pipe.
    // We will not run the next pipe if it contains input redirection
    (*prev_pipe_exists) = true;

    return command;
}

Command *parse_commands(char *line, Variable **variables) {

    // STEP 1: Find PATH Variable to pass in resolve_executable later
    Variable *path_var = *variables;
    while (path_var != NULL) {
      if (strcmp(path_var->name, PATH_VAR_NAME) == 0) {
        break;
      }
      path_var = path_var->next;
    }
    if (path_var == NULL) {
      ERR_PRINT(ERR_PATH_INIT, " ");
      return NULL;
    }

    /* STEP 2: Tokanize commands based on delimiter pipes ('|') and iteratively
     process (parse) them */
    char *pipe_token;
    char *token = strtok_r(line, "|", &pipe_token);
    if (token == NULL) {
      ERR_PRINT(ERR_PARSING_LINE);
      return NULL;
    }

    bool prev_pipe_exists = false;
    bool output_exists = false;

    Command *curr_command = parse_a_command(token, path_var, &prev_pipe_exists,
                                            &output_exists);

    // If NULL or -1, parse_a_command encountered an error
    if (curr_command == NULL) {
      return NULL;
    } if (curr_command == (Command *) -1) {
      return (Command *) -1;
    }

    // In order to not lose the first command after it's replaced with next ones
    Command *first_command = curr_command;

    Command *next_command;
    token = strtok_r(NULL, "|", &pipe_token);
    while (token != NULL) {
      if (output_exists) { // It means the last pipe contained > or >>,
        break;             // So, we are going to ignore the current piped token
      }
      next_command = parse_a_command(token, path_var, &prev_pipe_exists,
                                     &output_exists);
      if (next_command == NULL) {
        return NULL;
      } if (next_command == (Command *) -1) {
        return (Command *) -1;
      }
      curr_command->next = next_command;
      curr_command = curr_command->next;

      // Move on to the next token
      token = strtok_r(NULL, "|", &pipe_token);
    }

    return first_command;
}

Command *parse_variable_assignment(char *line, Variable **variables) {
    if (line[0] == '=') {
      // raise Error that '=' cannot be in the beginning
      ERR_PRINT(ERR_VAR_START);
      return NULL;
    }

    char *line_cpy = strdup(line);
    if (line_cpy == NULL) {
      perror("parse_variable_assignment");
      return (Command *) -1;
    }

    char *var_value = strchr(line_cpy, '=') + 1;
    char *var_name = strtok(line_cpy, "=");

    // Check validity of var_name
    for (int i = 0; i < strlen(var_name); i++) {
      bool is_capital_letter = ('A' <= var_name[i] && var_name[i] <= 'Z');
      bool is_small_letter = ('a' <= var_name[i] && var_name[i] <= 'z');
      bool is_underscore = ('_' == var_name[i]);
      if (!(is_capital_letter || is_small_letter || is_underscore)) {
        ERR_PRINT(ERR_VAR_NAME, var_name);
        free(line_cpy);
        return NULL;
      }
    }

    Variable *curr_var = *variables;

    // Traverse through the linked list to see if there is already var_name
    while (curr_var != NULL) {
      if (strcmp(curr_var->name, var_name) == 0) { // if current->name == name
        free(curr_var->value);

        curr_var->value = strdup(var_value);
        if (curr_var->value == NULL) {
          free(line_cpy);
          perror("parse_variable_assignment");
          return (Command *) -1;
        }

        free(line_cpy);
        return NULL;
      }
      curr_var = curr_var->next;
    }

    Variable *new_variable = malloc(sizeof(Variable));
    if (new_variable == NULL) {
      free(line_cpy);
      perror("parse_variable_assignment");
      return (Command *) -1;
    }
    new_variable->name = strdup(var_name);
    if (new_variable->name == NULL) {
      free(new_variable);
      free(line_cpy);
      perror("parse_variable_assignment");
      return (Command *) -1;
    }
    new_variable->value = strdup(var_value);
    if (new_variable->value == NULL) {
      free(new_variable->name);
      free(new_variable);
      free(line_cpy);
      perror("parse_variable_assignment");
      return (Command *) -1;
    }
    new_variable->next = *variables;

    *variables = new_variable;

    free(line_cpy);
    return NULL;
}

Command *parse_line(char *line, Variable **variables) {

    // Dynamically allocating, so we can modify in case it's from read-only mem.
    line = strdup(line);
    if (strlen(line) == 0) {
      ERR_PRINT(ERR_EXECUTE_LINE);
      free(line);
      return NULL;
    }

    // Remove the part including and after first #
    char* hashtag = strchr(line, '#');
    if (hashtag != NULL) {
      (*hashtag) = '\0';
    }

    if ((*line) == '\0') {
      ERR_PRINT(ERR_EXECUTE_LINE);
      free(line);
      return NULL;
    }

    line = clear_leading_whitespace(line);

    if (strlen(line) == 0) {
      ERR_PRINT(ERR_EXECUTE_LINE);
      return NULL;
    }

    char *ptr_to_equals = strchr(line, '=');

    /* No = in line (so it's a command execution) or = in line, but there is a
    space right before it (so it's a not variable assignment). */
    if (ptr_to_equals == NULL || isspace(ptr_to_equals[-1])) {

      char *new_line = replace_variables_mk_line(line, *variables);
      if (new_line == (char *) -1) {
        return (Command *) -1;
      } if (new_line == NULL) {
        return NULL;
      }

      Command *parsed_command = parse_commands(new_line, variables);

      // In case parse_commands returned due to an error.
      if (parsed_command == (Command *) -1) {
        free(new_line);
        return (Command *) -1;
      }

      if (parsed_command == NULL) {
        free(new_line);
        return NULL;
      }

      free(new_line);
      return parsed_command;

    // = in line and no space right before it. So, it is a variable assignment.
    } else {
      return parse_variable_assignment(line, variables);
    }
}


/*
** This function is partially implemented for you, but you may
** scrap the implementation as long as it produces the same result.
**
** Creates a new line on the heap with all named variable *usages*
** replaced with their associated values.
**
** Returns NULL if replacement parsing had an error, or (char *) -1 if
** system calls fail and the shell needs to exit.
*/
char *replace_variables_mk_line(const char *line, Variable *variables) {

    // NULL terminator accounted for here
    char new_line_ptr[MAX_SINGLE_LINE + 1]; // ptr to the start of our new_line
    char* new_line = new_line_ptr;


    const char *tracker = line;

    while (*tracker != '\0') {

      // If we hit '$', stop here
      if (*tracker == '$') {

        const char *parse_var_st, *parse_var_end;
        // We have two options: either ${smth} or $smth
        if (*(tracker + 1) == '{') {

          parse_var_st = tracker + 2; // ptr to the start of VAR_NAME
          parse_var_end = parse_var_st;

          while ( (*parse_var_end) != '}' && (*parse_var_end) != '\0' ) {
            parse_var_end++;
          }
          if ((*parse_var_end) == '\0') {
            ERR_PRINT(ERR_PARSING_LINE);
            return NULL;
          }

          int var_name_len = parse_var_end - parse_var_st + 1; // considering \0
          char var_name[var_name_len];
          strncpy(var_name, parse_var_st, var_name_len);
          var_name[var_name_len - 1] = '\0';

          Variable *curr_var = variables;
          while (curr_var != NULL) {
            if ( strcmp(curr_var->name, var_name) == 0 ) {
              strncat(new_line, curr_var->value, strlen(curr_var->value));
              new_line += strlen(curr_var->value);
              tracker += strlen(curr_var->name) + 3; // '$' + '{' + '}'
              break;
            }
            curr_var = curr_var->next;
          } if (curr_var == NULL) {
            ERR_PRINT(ERR_VAR_NOT_FOUND, var_name);
            return NULL;
          }
        } else {

          parse_var_st = tracker + 1; // ptr to the start of VAR_NAME
          parse_var_end = parse_var_st;

          while ( !(isspace(*parse_var_end)) && (*parse_var_end) != '\0'
                    && (*parse_var_end) != '$') {
            parse_var_end++;
          }

          int var_name_len = parse_var_end - parse_var_st + 1; // including \0
          char var_name[var_name_len];
          strncpy(var_name, parse_var_st, var_name_len);
          var_name[var_name_len - 1] = '\0';

          Variable *curr_var = variables;
          while (curr_var != NULL) {
            if ( strcmp(curr_var->name, var_name) == 0 ) {
              strncat(new_line, curr_var->value, strlen(curr_var->value));
              new_line += strlen(curr_var->value);
              tracker += strlen(curr_var->name) + 1; // + '$'
              break;
            }
            curr_var = curr_var->next;
          } if (curr_var == NULL) {
            ERR_PRINT(ERR_VAR_NOT_FOUND, var_name);
            return NULL;
          }
        }
      }
      if ( (*tracker) != '$' && (*tracker) != '\0' ) {
        (*new_line) = (*tracker);
        *(new_line+1) = '\0';
        new_line++;
        tracker++;
      } else {
        (*new_line) = '\0';
      }
    }

    char *new_line_malloced = strdup(new_line_ptr);
    if (new_line == NULL) {
        perror("replace_variables_mk_line");
        return (char *) -1;
    }

    return new_line_malloced;
}

void free_variable(Variable *var, uint8_t recursive) {

    Variable *curr = var;
    Variable *temp;

    while (curr == NULL) {
      temp = curr->next;
      free(curr->name);
      free(curr->value);
      free(curr);
      curr = temp;
    }
}
