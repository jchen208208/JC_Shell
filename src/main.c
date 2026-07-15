#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>

static bool is_builtin(const char *command) {
    return strcmp(command, "echo") == 0 ||
           strcmp(command, "exit") == 0 ||
           strcmp(command, "type") == 0 ||
           strcmp(command, "pwd") == 0 ||
           strcmp(command, "cd") == 0;
}

static char *find_in_path(const char *command) {
    char *path_env = getenv("PATH");

        if (path_env != NULL) {
            // Duplicate path_env because strtok modifies the string
            char *path_copy = strdup(path_env);
            char *dir = strtok(path_copy, ":");

            while (dir != NULL) {
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);

                // Check if file exists and is executable
                if (access(full_path, X_OK) == 0) {
                    char *result = strdup(full_path);
                    free(path_copy);
                    return result;
                }
                dir = strtok(NULL, ":");
            }
            free(path_copy);
        }

        return NULL;
    
    }

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);

    while (true) {
        printf("$ ");

        char input[100];
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        char *args[64];
        int nargs = 0;
        char token[1024];
        int len = 0;
        bool in_token = false;
        bool in_squote = false;

        for (int i = 0; input[i] != '\0'; i++) {
            char c = input[i];
            if (in_squote) {
                if (c == '\'') {
                    in_squote = false;
                }
                else {
                    token[len++] = c;
                }
            }
            else {
                if (c == '\'') {
                    in_squote = true;
                    in_token = true;
                }
                else if (c == ' ' || c == '\t') {
                    if (in_token) {
                        token[len] = '\0';
                        args[nargs++] = strdup(token);
                        len = 0;
                        in_token = false;
                    }
                    else {
                        continue;
                    }
                }
                else {
                    token[len++] = c;
                }
            }
        }

        if (in_token) {
            token[len] = '\0';
            args[nargs++] = strdup(token);
        }

        args[nargs] = NULL;
        if (args[0] == NULL) continue;


        if (strcmp(args[0], "exit") == 0) {
            break;
        }
        
        else if (strcmp(args[0], "echo") == 0) {
            for (int i = 1; i < nargs; i++) {
                printf("%s", args[i]);
                if (i < nargs - 1) {
                    printf(" ");   // space between, not after the last
                }
            }
            printf("\n");
        }

        else if (strcmp(args[0], "pwd") == 0) {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            }
        }

        else if (strcmp(args[0], "cd") == 0) {
            const char *path = args[1];
            if (strcmp(path, "~") == 0) {
                path = getenv("HOME");
            }
            if (chdir(path) != 0) {
                printf("cd: %s: No such file or directory\n", path);
            }
        }
        
        else if (strcmp(args[0], "type") == 0) {
            const char *command = args[1];
            if (is_builtin(command)) {
                printf("%s is a shell builtin\n", command);
            }
            else {
                char *full_path = find_in_path(command);
                if (full_path != NULL) {
                    printf("%s is %s\n", command, full_path);
                    free(full_path);
                }
                else {
                    printf("%s: not found\n", command);
                }
            }
        }
        
        else {
            char *full_path = find_in_path(args[0]);
            if (full_path == NULL) {
                printf("%s: command not found\n", args[0]);
            }
            else {
                
                pid_t pid = fork();

                if (pid == 0) {
                    // child, becomes the program
                    execv(full_path, args);

                    // only runs if execv failed
                    perror("execv");
                    exit(1);
                }
                else if (pid > 0) {
                    // paremt waits for the child to finish
                    int status;
                    waitpid(pid, &status, 0);
                }
                else {
                    perror("fork");
                }

                free(full_path);
            }
 
        }
    }

    return 0;
}