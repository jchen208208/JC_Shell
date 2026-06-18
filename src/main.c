#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static bool is_builtin(const char *command) {
    return strcmp(command, "echo") == 0 ||
           strcmp(command, "exit") == 0 ||
           strcmp(command, "type") == 0;
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
            } else {
                printf("%s: not found\n", command);
            }
        }
        
        else {
            printf("%s: command not found\n", input);
        }
    }

    return 0;
}