#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);
    
    while (true) {
      printf("$ ");

      // user input
      char input[100];
      fgets(input, 100, stdin);
      input[strlen(input) - 1] = '\0';
      
      if (strcmp(input, "exit") == 0) {
        break;
      }

      else if (strncmp(input, "echo ", 5) == 0) {
        printf("%s\n", input + 5);
      }
      
      else {
        printf("%s: command not found\n", input);
      }
  return 0;
}