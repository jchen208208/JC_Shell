#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>

static bool is_builtin(const char *command) {
    return strcmp(command, "echo") == 0 ||
           strcmp(command, "exit") == 0 ||
           strcmp(command, "type") == 0;
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

        if (strcmp(input, "exit") == 0) {
            break;
        }
        
        else if (strncmp(input, "echo ", 5) == 0) {
            printf("%s\n", input + 5);
        }
        
        else if (strncmp(input, "type ", 5) == 0) {
            const char *command = input + 5;
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
            char *args[64];
            int i = 0;
            args[0] = strtok(input, " ");

            if (args[0] == NULL) continue;
            
            while (args[i] != NULL) {
                i++;
                args[i] = strtok(NULL, " ");
            }

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