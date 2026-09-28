#define _POSIX_C_SOURCE 200112L  // Allows usage of setenv
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "lexer.h"

#define HISTORY_SIZE 3
static char *history[HISTORY_SIZE];
static int history_count = 0;

#define MAX_JOBS 64
typedef struct {
    int job_num;
    pid_t pid;
    char cmdline[200];
} job_t;

static job_t jobs[MAX_JOBS];
static int num_jobs = 0;
static int next_job_num = 1;


void add_to_history(const char *cmd) {
    if (history_count == HISTORY_SIZE) {
        free(history[0]);
        memmove(history, history + 1, (HISTORY_SIZE - 1) * sizeof(char *));
        history[HISTORY_SIZE - 1] = malloc(strlen(cmd) + 1);
        strcpy(history[HISTORY_SIZE - 1], cmd);
    }
    else {
        history[history_count] = malloc(strlen(cmd) + 1);
        strcpy(history[history_count], cmd);
        history_count++;
    }
}

void add_job(pid_t pid, const char *cmdline) {
    if (num_jobs >= MAX_JOBS) return;
    jobs[num_jobs].job_num = next_job_num++;
    jobs[num_jobs].pid = pid;
    strncpy(jobs[num_jobs].cmdline, cmdline, sizeof(jobs[num_jobs].cmdline) - 1);
    jobs[num_jobs].cmdline[sizeof(jobs[num_jobs].cmdline) - 1] = '\0';
    num_jobs++;
}

// Remove finished background jobs without blocking.
void remove_finished_jobs(void) {
    for (int i = 0; i < num_jobs; i++) {
        if (waitpid(jobs[i].pid, NULL, WNOHANG) > 0) {
            // shift remaining jobs down
            memmove(&jobs[i], &jobs[i + 1], (num_jobs - i - 1) * sizeof(job_t));
            num_jobs--;
            i--;
        }
    }
}

// part 1
void print_prompt() {
    char *user = getenv("USER");
    char *machine = getenv("MACHINE");
    char hostname[256];
    if (!machine) {
        if (gethostname(hostname, sizeof(hostname)) == 0)
            machine = hostname;
        else
            machine = "machine";
    }
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
// Returns 1 if the command should run in the background (last token was '&').
int check_background(tokenlist *tokens) {
    if (tokens->size > 0 && strcmp(tokens->items[tokens->size - 1], "&") == 0) {
        free(tokens->items[tokens->size - 1]);
        tokens->items[tokens->size - 1] = NULL;
        tokens->size--;
        return 1;
    }
    return 0;
}

// part 6
// Scans tokens for < and >, removes them and their filename arguments,
// and returns the filenames through `file_in` and `file_out`. Gives ownership to caller.
int parse_redirections(tokenlist *tokens, char **file_in, char **file_out) {
    *file_in = NULL;
    *file_out = NULL;

    for (int i = 0; i < (int)tokens->size; i++) {
        if (strcmp(tokens->items[i], ">") == 0 || strcmp(tokens->items[i], "<") == 0) {
            int is_out = (tokens->items[i][0] == '>');
            if (i + 1 >= (int)tokens->size) {
                fprintf(stderr, "syntax error: expected filename after '%c'\n", tokens->items[i][0]);
                return -1;
            }
            char *fname = malloc(strlen(tokens->items[i + 1]) + 1);
            strcpy(fname, tokens->items[i + 1]);
            free(tokens->items[i]);
            free(tokens->items[i + 1]);
            memmove(&tokens->items[i], &tokens->items[i + 2],
                    (tokens->size - i - 2) * sizeof(char *));
            tokens->size -= 2;
            tokens->items[tokens->size] = NULL;
            i--;
            if (is_out)
                *file_out = fname;
            else
                *file_in = fname;
        }
    }
    return 0;
}

// Splits tokens on "|" into an array of tokenlists, caller is given ownership of each as well as the array.
int split_on_pipe(tokenlist *tokens, tokenlist ***out_segs) {
    int n = 1;
    for (int i = 0; i < (int)tokens->size; i++)
        if (strcmp(tokens->items[i], "|") == 0) n++;

    tokenlist **segs = malloc(n * sizeof(tokenlist *));
    for (int i = 0; i < n; i++) segs[i] = new_tokenlist();

    int seg = 0;
    for (int i = 0; i < (int)tokens->size; i++) {
        if (strcmp(tokens->items[i], "|") == 0)
            seg++;
        else
            add_token(segs[seg], tokens->items[i]);
    }

    *out_segs = segs;
    return n;
}

// part 7
void execute_piped(tokenlist **segs, int n, const char *raw_input, int background) {
    int (*pipefds)[2] = malloc((n - 1) * sizeof(*pipefds));
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipefds[i]) == -1) {
            perror("pipe");
            for (int j = 0; j < i; j++) { close(pipefds[j][0]); close(pipefds[j][1]); }
            free(pipefds);
            return;
        }
    }

    pid_t *pids = malloc(n * sizeof(pid_t));

    for (int i = 0; i < n; i++) {
        // segs[0] was already resolved by main; resolve the rest here
        if (i > 0) resolve_path(segs[i]);

        char *file_in = NULL, *file_out = NULL;
        if (parse_redirections(segs[i], &file_in, &file_out) != 0) {
            free(file_in); free(file_out);
            pids[i] = -1;
            continue;
        }

        if (segs[i]->size == 0 || segs[i]->items[0][0] == '\0') {
            free(file_in); free(file_out);
            pids[i] = -1;
            continue;
        }

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            free(file_in); free(file_out);
            pids[i] = -1;
            continue;
        }

        if (pid == 0) {
            // Wire stdin from previous pipe unless overridden by <
            if (i > 0 && file_in == NULL) {
                dup2(pipefds[i - 1][0], STDIN_FILENO);
            }
            // Wire stdout to next pipe unless overridden by >
            if (i < n - 1 && file_out == NULL) {
                dup2(pipefds[i][1], STDOUT_FILENO);
            }

            for (int j = 0; j < n - 1; j++) {
                close(pipefds[j][0]);
                close(pipefds[j][1]);
            }

            if (file_in) {
                struct stat st;
                if (stat(file_in, &st) != 0) { fprintf(stderr, "%s: No such file or directory\n", file_in); exit(1); }
                if (!S_ISREG(st.st_mode)) { fprintf(stderr, "%s: Not a regular file\n", file_in); exit(1); }
                int fd = open(file_in, O_RDONLY);
                if (fd == -1) { perror(file_in); exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (file_out) {
                int fd = open(file_out, O_WRONLY | O_CREAT | O_TRUNC, 0600);
                if (fd == -1) { perror(file_out); exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            execv(segs[i]->items[0], segs[i]->items);
            perror("execv");
            exit(1);
        }

        pids[i] = pid;
        free(file_in);
        free(file_out);
    }

    for (int i = 0; i < n - 1; i++) {
        close(pipefds[i][0]);
        close(pipefds[i][1]);
    }

    if (background) {
        add_job(pids[n - 1], raw_input);
    }
    else {
        for (int i = 0; i < n; i++) {
            if (pids[i] > 0) waitpid(pids[i], NULL, 0);
        }
    }

    free(pipefds);
    free(pids);
}

void execute_command(tokenlist *tokens, const char *raw_input) {
    if (tokens->size == 0) return;

    int background = check_background(tokens);
    if (tokens->size == 0) return;

    // Check for pipes before the empty-command guard
    int has_pipe = 0;
    for (int i = 0; i < (int)tokens->size; i++)
        if (strcmp(tokens->items[i], "|") == 0) { has_pipe = 1; break; }

    if (has_pipe) {
        tokenlist **segs;
        int n = split_on_pipe(tokens, &segs);
        execute_piped(segs, n, raw_input, background);
        for (int i = 0; i < n; i++) free_tokens(segs[i]);
        free(segs);
        return;
    }

    if (tokens->items[0][0] == '\0') return;

    char *file_in = NULL, *file_out = NULL;
    if (parse_redirections(tokens, &file_in, &file_out) != 0) {
        free(file_in);
        free(file_out);
        return;
    }
    if (tokens->size == 0 || tokens->items[0][0] == '\0') {
        free(file_in);
        free(file_out);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
    }
    else if (pid == 0) {
        if (file_out) {
            int fd = open(file_out, O_WRONLY | O_CREAT | O_TRUNC, 0600);
            if (fd == -1) { perror(file_out); exit(1); }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (file_in) {
            struct stat st;
            if (stat(file_in, &st) != 0) {
                fprintf(stderr, "%s: No such file or directory\n", file_in);
                exit(1);
            }
            if (!S_ISREG(st.st_mode)) {
                fprintf(stderr, "%s: Not a regular file\n", file_in);
                exit(1);
            }
            int fd = open(file_in, O_RDONLY);
            if (fd == -1) { perror(file_in); exit(1); }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        execv(tokens->items[0], tokens->items);
        perror("execv failed");
        exit(1);
    } else {
        if (background) {
            add_job(pid, raw_input);
        } else {
            waitpid(pid, NULL, 0);
        }
    }

    free(file_in);
    free(file_out);
}

// Internal cd command
void builtin_cd(tokenlist *tokens) {
    char *target;
    if (tokens->size == 1) {
        target = getenv("HOME");
        if (!target) {
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
    }
    else if (tokens->size > 2) {
        fprintf(stderr, "cd: too many arguments\n");
        return;
    }
    else {
        target = tokens->items[1];
    }

    struct stat st;
    if (stat(target, &st) != 0) {
        fprintf(stderr, "cd: %s: No such file or directory\n", target);
        return;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "cd: %s: Not a directory\n", target);
        return;
    }
    if (chdir(target) != 0) {
        perror("cd");
        return;
    }

    // Update pwd
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        setenv("PWD", cwd, 1);
    }
}

// Internal jobs command
void builtin_jobs(void) {
    remove_finished_jobs();
    if (num_jobs == 0) {
        printf("No active background processes\n");
        return;
    }
    for (int i = 0; i < num_jobs; i++) {
        printf("[%d]+ %d %s\n", jobs[i].job_num, jobs[i].pid, jobs[i].cmdline);
    }
}

// Internal exit command
void builtin_exit(void) {
    // Wait for remaining jobs to finish
    for (int i = 0; i < num_jobs; i++) {
        waitpid(jobs[i].pid, NULL, 0);
    }

    if (history_count == 0) {
        printf("No commands in history\n");
    }
    else {
        int count = history_count < HISTORY_SIZE ? history_count : HISTORY_SIZE;
        int display = count < 3 ? 1 : 3;
        for (int i = count - display; i < count; i++) {
            printf("%s\n", history[i]);
        }
    }

    exit(0);
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

        if (tokens->size == 0) {
            free(input);
            free_tokens(tokens);
            continue;
        }

        // handle internal commands
        if (strcmp(tokens->items[0], "exit") == 0) {
            add_to_history(input);
            free(input);
            free_tokens(tokens);
            builtin_exit();
        }
        else if (strcmp(tokens->items[0], "cd") == 0) {
            add_to_history(input);
            builtin_cd(tokens);
        }
        else if (strcmp(tokens->items[0], "jobs") == 0) {
            add_to_history(input);
            builtin_jobs();
        }
        else {
            // expands special characters
            expand_env_vars(tokens);
            expand_tilde(tokens);
            resolve_path(tokens);

            // only add to history if command was found
            if (tokens->items[0][0] != '\0') {
                add_to_history(input);
            }

            // Finds executables and runs it
            execute_command(tokens, input);
        }

        // cleans up memory
        free(input);
        free_tokens(tokens);
    }
    return 0;
}
