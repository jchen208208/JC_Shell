#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>

// checks if the command is builtin for type command
static bool is_builtin(const char *command) {
    return strcmp(command, "echo") == 0 ||
           strcmp(command, "exit") == 0 ||
           strcmp(command, "type") == 0 ||
           strcmp(command, "pwd") == 0 ||
           strcmp(command, "cd") == 0;
}

// finds the absolute path of a file
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

// line reading function
static int read_line(char *buf, int size) {
    struct termios orig, raw;
    tcgetattr(0, &orig);
    raw = orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &raw);

    // reading loop
    int len = 0;
    int result = 0;
    char c;

    while (true) {
        if (read(0, &c, 1) != 1) {
            result = -1;
            break;
        }
        if (c == '\n') {
            puts("");
            break;
        }
        if (c == '\t') {
            const char candidates[][24] = {"echo", "exit"};
            const char *match = NULL;
            int count = 0;

            for (int i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); i++) {
                if (strncmp(candidates[0], buf, len) == 0) {
                    match = candidates[i];
                    count++;
                }
            }

            if (count == 1) {
                printf("%s ", match + len + 1);
                strcpy(buf, match);
                len = strlen(match);
                buf[len++] = ' ';
            }

            continue;
        }
        if (len < size - 1) {
            buf[len++] = c;
            printf("%c", c);
        }
    }

    buf[len] = '\0';
    tcsetattr(0, TCSANOW, &orig);
    return result;
}

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);

    while (true) {
        printf("$ ");

        // take cli input
        char input[100];
        if (read_line(input, sizeof(input)) < 0) {
            break;
        }


        // tokenization step (breaking input into arguments array)
        char *args[64];
        int nargs = 0;
        char token[1024];
        int len = 0;
        bool in_token = false;
        bool in_squote = false;
        bool in_dquote = false;

        for (int i = 0; input[i] != '\0'; i++) {
            char c = input[i];
            if (in_dquote) {
                if (c == '\"') {
                    in_dquote = false;
                }
                else if (c == '\\') {
                    if (input[i + 1] != '\0') {
                        token[len++] = input[i + 1];
                        in_token = true;
                        i++;
                    }
                }
                else {
                    token[len++] = c;
                }
            }
            else if (in_squote) {
                if (c == '\'') {
                    in_squote = false;
                }
                else {
                    token[len++] = c;
                }
            }
            else {
                if (c == '\\') {
                    if (input[i + 1] != '\0') {
                        token[len++] = input[i + 1];
                        in_token = true;
                        i++;
                    }
                }
                else if (c == '\'') {
                    in_squote = true;
                    in_token = true;
                }
                else if (c == '\"') {
                    in_dquote = true;
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
                    in_token = true;
                }
            }
        }

        if (in_token) {
            token[len] = '\0';
            args[nargs++] = strdup(token);
        }

        args[nargs] = NULL;
        if (args[0] == NULL) continue;

        // checks for standard output and standard error redirections
        char *out_filename = NULL;
        bool out_append = false;
        char *err_filename = NULL;
        bool err_append = false;

        int args_end = nargs;

        for (int i = 0; i < args_end; i++) {
            if ((strcmp(args[i], ">") == 0) || (strcmp(args[i], "1>") == 0)) {
                out_filename = args[i + 1];
                if (i < nargs) {
                    nargs = i;
                }
                args[i] = NULL;
            }

            else if ((strcmp(args[i], "2>") == 0)) {
                err_filename = args[i + 1];
                if (i < nargs) {
                    nargs = i;
                }
                args[i] = NULL;
            }

            else if ((strcmp(args[i], ">>") == 0) || (strcmp(args[i], "1>>") == 0)) {
                out_filename = args[i + 1];
                out_append = true;
                if (i < nargs) {
                    nargs = i;
                }
                args[i] = NULL;
            }

             else if (strcmp(args[i], "2>>") == 0) {
                err_filename = args[i + 1];
                err_append = true;
                if (i < nargs) {
                    nargs = i;
                }
                args[i] = NULL;
            }
        }

        

        // redirect stdout to the file for builtins, saving the terminal fd to restore after
        int saved_stdout = -1;
        int saved_stderr = -1;

        if (out_filename != NULL && is_builtin(args[0])) {
            int fd = open(out_filename, O_WRONLY | O_CREAT | (out_append ? O_APPEND : O_TRUNC), 0644);

            if (fd < 0) {
                perror(out_filename);
                continue;
            }

            saved_stdout = dup(1);
            dup2(fd, 1);
            close(fd);
        }

        // redirect stderr for builtins
        if (err_filename != NULL && is_builtin(args[0])) {
            int fd = open(err_filename, O_WRONLY | O_CREAT | (err_append ? O_APPEND : O_TRUNC), 0644);

            if (fd < 0) {
                perror(err_filename);
                continue;
            }

            saved_stderr = dup(2);
            dup2(fd, 2);
            close(fd);
        }

        // exits the loop
        if (strcmp(args[0], "exit") == 0) {
            break;
        }
        
        // restates everything after "echo"
        else if (strcmp(args[0], "echo") == 0) {
            for (int i = 1; i < nargs; i++) {
                printf("%s", args[i]);
                if (i < nargs - 1) {
                    printf(" ");   // space between args, not after the last
                }
            }
            printf("\n");
        }

        // returns current working directory
        else if (strcmp(args[0], "pwd") == 0) {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                printf("%s\n", cwd);
            }
        }

        // change directory to input path
        else if (strcmp(args[0], "cd") == 0) {
            const char *path = args[1];
            if (strcmp(path, "~") == 0) {
                path = getenv("HOME");
            }
            if (chdir(path) != 0) {
                printf("cd: %s: No such file or directory\n", path);
            }
        }
        
        // determines the type of the input (builtin, an executable file, or invalid)
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
        
        // if the first argument of input is an executable file, run that process using a child process and taking in the rest of the arguments as the child process's arguments
        else {
            char *full_path = find_in_path(args[0]);
            if (full_path == NULL) {
                printf("%s: command not found\n", args[0]);
            }
            else {
                
                pid_t pid = fork();

                if (pid == 0) {
                    // child, becomes the program

                    if (out_filename != NULL) {
                        int fd = open(out_filename, O_WRONLY | O_CREAT | (out_append ? O_APPEND : O_TRUNC), 0644);

                        if (fd < 0) {
                            perror(out_filename);
                            exit(1);
                        }

                        dup2(fd, 1);

                        close(fd);
                    }

                    if (err_filename != NULL) {
                        int fd = open(err_filename, O_WRONLY | O_CREAT | (err_append ? O_APPEND : O_TRUNC), 0644);

                        if (fd < 0) {
                            perror(err_filename);
                            exit(2);
                        }

                        dup2(fd, 2);

                        close(fd);
                    }

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

        // restores stdout to the terminal after a builtin redirect
        if (saved_stdout != -1) {
            dup2(saved_stdout, 1);
            close(saved_stdout);
        }

        if (saved_stderr != -1) {
            dup2(saved_stderr, 2);
            close(saved_stderr);
        }
    }

    return 0;
}