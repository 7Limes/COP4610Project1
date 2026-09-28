#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "lexer.h"

// part 1
void print_prompt() {
    char *user = getenv("USER");
    char *machine = getenv("MACHINE");
    char *pwd = getenv("PWD");
    if (!user) user = "user";
    if (!machine) machine = "machine";
    if (!pwd) pwd = "pwd";
    printf("%s@%s:%s> ", user, machine, pwd);
    fflush(stdout);
}

// part 2
void expand_env_vars(tokenlist *tokens) {
    for (int i = 0; i < tokens->size; i++) {
        if (tokens->items[i][0] == '$') {
            char *env_val = getenv(tokens->items[i] + 1);
            free(tokens->items[i]);
            if (env_val) {
                tokens->items[i] = malloc(strlen(env_val) + 1);
                strcpy(tokens->items[i], env_val);
            } else {
                tokens->items[i] = malloc(1);
                tokens->items[i][0] = '\0';
            }
        }
    }
}

// part 3
void expand_tilde(tokenlist *tokens) {
    char *home = getenv("HOME");
    if (!home) return;

    for (int i = 0; i < tokens->size; i++) {
        if (strcmp(tokens->items[i], "~") == 0) {
            free(tokens->items[i]);
            tokens->items[i] = malloc(strlen(home) + 1);
            strcpy(tokens->items[i], home);
        } else if (strncmp(tokens->items[i], "~/", 2) == 0) {
            char *new_path = malloc(strlen(home) + strlen(tokens->items[i]));
            sprintf(new_path, "%s%s", home, tokens->items[i] + 1);
            free(tokens->items[i]);
            tokens->items[i] = new_path;
        }
    }
}

// part 4
void resolve_path(tokenlist *tokens) {
    if (tokens->size == 0 || tokens->items[0][0] == '\0') return;
    
    char *cmd = tokens->items[0];

    // If the command is an absolute or relative path, check F_OK as required by slides
    if (strchr(cmd, '/') != NULL) {
        if (access(cmd, F_OK) != 0) {
            printf("Command not found: %s\n", cmd);
        }
        return; 
    }
    
    char *path_env = getenv("PATH");
    if (!path_env) return;
 
    char *path_copy = malloc(strlen(path_env) + 1);
    strcpy(path_copy, path_env);
    
    char *dir = strtok(path_copy, ":");
    char buffer[4096];
    int found = 0;

    while (dir != NULL) {
        snprintf(buffer, sizeof(buffer), "%s/%s", dir, cmd);
        
        if (access(buffer, F_OK) == 0) {
            found = 1; 
            free(tokens->items[0]);
            tokens->items[0] = malloc(strlen(buffer) + 1);
            strcpy(tokens->items[0], buffer);
            break;
        }
        dir = strtok(NULL, ":");
    }
    
    if (!found) {
        printf("Command not found: %s\n", cmd);
        // Clear the token so we don't try to execute it
        free(tokens->items[0]);
        tokens->items[0] = malloc(1);
        tokens->items[0][0] = '\0';
    }
    
    free(path_copy);
}
// part 5
void execute_command(tokenlist *tokens) {
    if (tokens->size == 0 || tokens->items[0][0] == '\0') return;

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
    } else if (pid == 0) {
        // execv must take the absolute path which is now stored inside tokens->items[0] 
        execv(tokens->items[0], tokens->items);
        perror("execv failed");
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
    }
}
int main() {
    while (1) {
        print_prompt();

        // gets user input
        char *input = get_input();

        // EOF (Ctrl+D) to exit the program
        if (input == NULL || feof(stdin)) {
            if (input) free(input);
            printf("\n");
            break; 
        }

        // Entering an empty prompt shows print_prompt again
        if (strlen(input) == 0) {
            free(input);
            continue;
        }

        // tokenizes raw input
        tokenlist *tokens = get_tokens(input);

        // expands special characters
        expand_env_vars(tokens);
        expand_tilde(tokens);
        resolve_path(tokens);
        
        // Finds executables and runs it
        execute_command(tokens);

        // cleans up memory
        free(input);
        free_tokens(tokens);
    }
    return 0;
}
