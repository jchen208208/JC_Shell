#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    setbuf(stdout, NULL);
    while (1) {
        printf("$ ");
        char input[100];
        if (fgets(input, 100, stdin) == NULL) break;
        input[strcspn(input, "\n")] = '\0';
        printf("%s: command not found\n", input);
    }
    return 0;
}
