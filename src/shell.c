// Needed to add this #definebecause the Makefile uses C99 and I wanted to change the least amount of code possible outside of this script.
#define _POSIX_C_SOURCE 200809L

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
char* resolve_path(char *cmd) {
    if (strchr(cmd, '/') != NULL) {
        if (access(cmd, X_OK) == 0) {
            return strdup(cmd);
        }
        return NULL;
    }
    char *path_env = getenv("PATH");
    if (!path_env) return NULL;

    char *path_copy = strdup(path_env);
    char *dir = strtok(path_copy, ":");
    char buffer[4096];

    while (dir != NULL) {
        snprintf(buffer, sizeof(buffer), "%s/%s", dir, cmd);
        if (access(buffer, X_OK) == 0) {
            free(path_copy);
            return strdup(buffer);
        }
        dir = strtok(NULL, ":");
    }
    free(path_copy);
    return NULL;
}

// part 5
void execute_command(tokenlist *tokens) {
    if (tokens->size == 0 || tokens->items[0][0] == '\0') return;

    char *cmd_path = resolve_path(tokens->items[0]);
    if (!cmd_path) {
        printf("Command not found: %s\n", tokens->items[0]);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
    } else if (pid == 0) {
        execv(cmd_path, tokens->items);
        perror("execv failed");
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
    }

    free(cmd_path);
}

int main() {
    while (1) {
        print_prompt();

        // gets user input
        char *input = get_input();

        // EOF (Ctrl+D) to exit the program
        if (input == NULL || strlen(input) == 0) {
            if (input) free(input);
            printf("\n");
            break; 
        }

        // tokenizes raw input
        tokenlist *tokens = get_tokens(input);

        // expands special characters
        expand_env_vars(tokens);
        expand_tilde(tokens);
        
        // Finds executables and runs it
        execute_command(tokens);

        // cleans up memory
        free(input);
        free_tokens(tokens);
    }
    return 0;
}
