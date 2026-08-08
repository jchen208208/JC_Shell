#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <termios.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>


// checks if the command is builtin for type command
static bool is_builtin(const char *command) {
    return strcmp(command, "echo") == 0 ||
           strcmp(command, "exit") == 0 ||
           strcmp(command, "type") == 0 ||
           strcmp(command, "pwd") == 0 ||
           strcmp(command, "cd") == 0 ||
           strcmp(command, "complete") == 0 ||
           strcmp(command, "jobs") == 0 ||
           strcmp(command, "history") == 0 ||
           strcmp(command, "declare") == 0;
}

// returns the absolute path of an executable file
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

// helper check duplicates function
static bool already_have(char list[][256], int n, const char *name) {
    for (int i = 0; i < n; i++)
        if (strcmp(list[i], name) == 0) return true;
    return false;
}

// helper sort function
static int cmp_name(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

// struct for storing programmable completion data
typedef struct {
    char command[64];
    char spec[1024];
} complete_spec;

// array of completion specs
static complete_spec specs[64];
static int nspecs = 0;

// finds the completion for a specific command
static char *find_spec(const char* command) {
    for (int i = 0; i < nspecs; i++) {
        if (strcmp(specs[i].command, command) == 0) {
            return specs[i].spec;
        }
    }
    return NULL;
}

// struct for storing data on background processes
typedef struct {
    int number;
    pid_t pid;
    char command[128];
} job;

// array of background jobs
static job jobs[64];
static int njobs = 0;

// reaping before each prompt (only displays Done jobs)
static void reap_jobs(bool show_running) {
    int w = 0;   // write index in the jobs array

    for (int i = 0; i < njobs; i++) {
        char sign = ' ';
                if (i == njobs - 1) {
                    sign = '+';
                }
                else if (i == njobs - 2) {
                    sign = '-';
                }

        int running = waitpid(jobs[i].pid, NULL, WNOHANG); // WNOHANG means wait don't hang so the parent process still runs while waiting for the child to finish
        // waitpid with WNOHANG returns 0 if the child process is still runing and returns the child pid (> 0) if the child process has exited, without WNOHANG, the parent waits until the child finishes so it can only output the pid when it's done
        if (running == 0) {
            // if this function is called by jobs command, print the running processes
            if (show_running) {
                printf("[%d]%c  %-24s%s &\n", jobs[i].number, sign, "Running", jobs[i].command);
            }
            // else, don't print anything
            jobs[w++] = jobs[i];
        }

        else {
            printf("[%d]%c  %-24s%s\n", jobs[i].number, sign, "Done", jobs[i].command);
            // don't save the process back  to the jobs list, no w++
        }
    }
    njobs = w;
}

// struct for storing commands in history
typedef struct {
    int order;
    char command[128];
} history;

static history history_list[64];
static int nhistory;
static int last_appended; // the index of the first entry that hasn't been appended to the history file yet

// used for loading the history file contents into the history array
static void load_history(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return;
    }

    char line[128];
    while (nhistory < 64 && fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') {
            continue; // empty line = end of file
        }
        history_list[nhistory].order = nhistory + 1;
        snprintf(history_list[nhistory].command, sizeof(history_list[nhistory].command), "%s", line);
        nhistory++;
    }
    fclose(f);
}

// saves history to the history file (either write or append)
static void save_history(const char *path, const char *mode, int start) {
    FILE *f = fopen(path, mode); // "w" for creating or overwriting the entire file, "a" for appending to the end of the file
    if (!f) {
        return;
    }

    for (int i = start; i < nhistory; i++) {
        fprintf(f, "%s\n", history_list[i].command);
    }

    fclose(f);
    last_appended = nhistory;
}

// runs any built-in commands. this is a refactor since the pipe feature should work for built-in commands as well
static void run_builtin(char **args, int nargs) {
    // restates everything after "echo"
    if (strcmp(args[0], "echo") == 0) {
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

    // register programmable completions for commands like git
    else if (strcmp(args[0], "complete") == 0) {
        if (strcmp(args[1], "-C") == 0 && nargs >= 4) {
            // args[2] = script path, args[3] = command name
            int i = 0;
            // finds the next free index if command not already in array; else, points to the command so we can overwrite the current completion spec
            while (i < nspecs && strcmp(specs[i].command, args[3]) != 0) {
                i++;
            }

            if (i < 64) {
                snprintf(specs[i].command, sizeof(specs[i].command), "%s", args[3]);
                snprintf(specs[i].spec, sizeof(specs[i].spec), "%s", args[2]);
                if (i == nspecs) {
                    nspecs++;
                }
            }
        }

        else if (strcmp(args[1], "-r") == 0 && nargs >= 3) {
            for (int i = 0; i < nspecs; i++) {
                if (strcmp(specs[i].command, args[2]) == 0) {
                    specs[i].command[0] = '\0';
                }
            }
        }

        else if (strcmp(args[1], "-p") == 0 && nargs >= 3) {
            const char* spec = find_spec(args[2]);
            // if no command specification found
            if (spec == NULL) {
                printf("complete: %s: no completion specification\n", args[2]);
            }

            else {
                printf("complete -C '%s' %s\n", spec, args[2]);
            }
        }
    }

    else if (strcmp(args[0], "jobs") == 0) {
        // jobs will only show Done if the process finished while the user is typing the prompt. Otherwise, the pre-prompt reap_jobs() will display Done and the process is gone by the time the jobs command is called
        reap_jobs(true); // display the running commands
    }

    else if (strcmp(args[0], "history") == 0) {
        if (args[1]) {
            if (strcmp(args[1], "-r") == 0 && nargs >= 3) {
                load_history(args[2]);
            }

            else if (strcmp(args[1], "-w") == 0 && nargs >= 3) {
                save_history(args[2], "w", 0);
            }

            else if (strcmp(args[1], "-a") == 0 && nargs >= 3) {
                save_history(args[2], "a", last_appended);
            }

            else {
                int i = atoi(args[1]);
                int start = nhistory - i;
                if (start < 0) {
                    start = 0;
                }
                for (int i = start; i < nhistory; i++) {
                    printf("%5d%c %s\n", history_list[i].order, ' ', history_list[i].command);
                }
            }
        }

        else {
            for (int i = 0; i < nhistory; i++) {
                printf("%5d%c %s\n", history_list[i].order, ' ', history_list[i].command);
            }
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
}

// runs one pipeline stage inside a child process after we fork them for the pipe
static void run_stage(char **argv, int argc) {
    if (is_builtin(argv[0])) {
        run_builtin(argv, argc);
        exit(0);
    }

    char *path = find_in_path(argv[0]);
    if (path == NULL) {
        fprintf(stderr, "%s: command not found\n", argv[0]);
        exit(127);
    }

    execv(path, argv);

    // if exec fails:
    perror("execv");
    exit(1);
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
    bool prev_tab = false;
    int history_pos = nhistory;


    while (true) {
        if (read(0, &c, 1) != 1) {
            result = -1;
            prev_tab = false;
            break;
        }

        // if enter is pressed, return the string
        if (c == '\n') {
            puts("");
            prev_tab = false;
            break;
        }

        if (c == '\t') {
            // check if we are completing the first word or second word
            int word_start = 0;
            for (int i = 0; i < len; i++) {
                if (buf[i] == ' ') {
                    word_start = i + 1;
                }
            }

            int match_start = word_start;

            // auto-completion for builtin commands
            const char builtins[][24] = {"echo", "exit"};
            char match[64][256];
            int count = 0;

            // completing first word (builtin command or executable file)
            if (word_start == 0) {
                for (int i = 0; i < (sizeof(builtins) / sizeof(builtins[0])); i++) {
                    if (strncmp(builtins[i], buf, len) == 0) {
                        strcpy(match[count++], builtins[i]);
                    }
                }

                // auto-completion for executatble files
                char *path_copy = strdup(getenv("PATH"));
                char *dir = strtok(path_copy, ":");
                while (dir != NULL) {
                    DIR *d = opendir(dir);
                    if (d != NULL) {
                        struct dirent *entry;
                        while ((entry = readdir(d)) != NULL) {
                            if (strncmp(entry->d_name, buf, len) == 0) {
                                if (already_have(match, count, entry->d_name)) {
                                    continue;
                                }
                                strcpy(match[count++], (*entry).d_name);
                            }
                        }
                        closedir(d);
                    }
                    dir = strtok(NULL, ":");
                }
                free(path_copy);
            }
            
            // completing second word (regular file)
            else {
                // programmable command completion
                char cmd[64];
                int cmd_len = 0;
                while (cmd_len < len && buf[cmd_len] != ' ' && cmd_len < (int)sizeof(cmd) - 1) {
                    cmd_len++;
                }
                snprintf(cmd, sizeof(cmd), "%.*s", cmd_len, buf);
                char *spec = find_spec(cmd);
                
                if (spec != NULL) {
                    int word_start = 0;
                    int prev_start = 0;
                    for (int i = 0; i < len; i++) {
                        if (buf[i] == ' ') {
                            prev_start = word_start;
                            word_start = i + 1;
                        }
                    }
                    
                    // creates the three arguments to pass to the completer program
                    char cmdline[2048];
                    snprintf(cmdline, sizeof(cmdline), "COMP_LINE='%.*s' COMP_POINT=%d %s '%s' '%.*s' '%.*s'", len, buf, len, spec, cmd, len - word_start, buf + word_start, word_start - prev_start - 1, buf + prev_start);
                    
                    FILE *fp = popen(cmdline, "r");
                    if (fp != NULL) {
                        char line[256];
                        while (count < 64 && fgets(line, sizeof(line), fp) != NULL) {
                            line[strcspn(line, "\n")] = '\0';
                            if (line[0] != '\0') {
                                sprintf(match[count], "%s", line);
                                count++;
                            }
                        }
                        pclose(fp);
                    }
                }

                else {
                    char dirpath[1024] = ".";
                    int last_slash = -1;

                    for (int i = word_start; i < len; i++) {
                        if (buf[i] == '/') {
                            last_slash = i;
                        }
                    }

                    if (last_slash >= 0) {
                        memcpy(dirpath, buf + word_start, last_slash - word_start + 1);
                        dirpath[last_slash - word_start + 1] = '\0';
                        match_start = last_slash + 1;
                    }

                    DIR *d = opendir(dirpath);
                    if (d != NULL) {
                        struct dirent *entry;
                        while ((entry = readdir(d)) != NULL) {
                            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                                continue;
                            }
                            if (strncmp(entry->d_name, buf + match_start, len - match_start) == 0) {
                                strcpy(match[count], entry->d_name);
                                char full_path[2048];
                                snprintf(full_path, sizeof(full_path), "%s/%s", dirpath, entry->d_name);

                                struct stat st;
                                if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                                    strcat(match[count], "/");
                                }
                                count++;
                            }
                        }
                        closedir(d);
                    }
                }
            }

            // sorts the list of matches in alphabetical worder
            qsort(match, count, sizeof(match[0]), cmp_name);

            // finding the longest common prefix amongst the matches
            int lcp = (count > 1) ? strlen(match[0]) : 0;
            for (int i = 1; i < count; i++) {
                int j = 0;
                while (j < lcp && match[i][j] == match[0][j]) {
                    j++;
                }
                lcp = j;
            }

            if (count == 1) {
                size_t match_len = strlen(match[0]);
                bool is_dir = match[0][match_len - 1] == '/';

                printf("%s", match[0] + (len - match_start));
                strcpy(buf + match_start, match[0]);
                len = match_start + match_len;

                if (!is_dir) {
                    printf(" ");
                    buf[len++] = ' ';
                }
            }

            if (count == 0) {
                printf("\x07"); // beep sound
            }

            if (count > 1) {
                if (lcp > (len - match_start)) {
                    printf("%.*s", lcp - (len - match_start), match[0] + (len - match_start)); // only show the characters after len but before the lcp mark for partial completion
                    for (int i = len -  match_start; i < lcp; i++) {
                        buf[match_start + i] = match[0][i];
                    }
                    len = match_start + lcp;
                    prev_tab = false;
                }

                else {
                    if (!prev_tab) {
                        printf("\x07");
                        prev_tab = true;
                    }
                    else if (prev_tab) {
                        puts("");
                        for (int i = 0; i < count; i++) {
                            printf("%s", match[i]);
                            if (i < count - 1) {
                                printf("  ");
                            }
                        }
                        // reprints what the user had typed before pressing tab twice. .* is filled by len as the max length of the string since buf isn't null terminated yet
                        printf("\n$ %.*s", len, buf);
                        prev_tab = false;
                    }
                }
            }

            continue;
        }

        // checks for up & down arrow navigation through history
        if (c == 0x1b) {
            char seq[2]; // arrows are denoted by 3 bytes, 0x1b, [, A / B (up / down)
            // read(fd, buffer, max_bytes)
            if (read(0, &seq[0], 1) != 1 || read(0, &seq[1], 1) != 1) {
                continue;
            }

            if (seq[0] == '[' && seq[1] == 'A') {
                // up arrow
                if (history_pos > 0) {
                    history_pos--;
                    int n = snprintf(buf, size, "%s", history_list[history_pos].command);
                    len = (n < size) ? n : size;
                    // display the command from history
                    printf("\r\x1b[K$ %.*s", len, buf); // '\r' moves the cursor to the front of the line, '\x1b[k' erases the chars from the cursor to the end of the line
                }
            }

            if (seq[0] == '[' && seq[1] == 'B') {
                // down arrow
                if (history_pos > 0 && history_pos < nhistory) {
                    history_pos++;
                    int n = snprintf(buf, size, "%s", history_list[history_pos].command);
                    len = (n < size) ? n : size;
                    printf("\r\x1b[K$ %.*s", len, buf);
                }
            }

            continue;
        }
        
        if (len < size - 1) {
            buf[len++] = c;
            prev_tab = false;
            printf("%c", c);
        }
    }

    buf[len] = '\0';
    tcsetattr(0, TCSANOW, &orig);
    return result;
}


int main(int argc, char *argv[]) {
    setbuf(stdout, NULL);

    const char *histfile = getenv("HISTFILE");
    // if the environment variable is set, then load its contents into the history array
    if (histfile) {
        load_history(histfile);
        last_appended = nhistory; // whatever got loaded from the file is already in the file so you want to adjust the last appended index to the number of histories now in the array
    }


    while (true) {
        // reaps jobs before each new prompt
        reap_jobs(false); // never shows running jobs

        printf("$ ");

        // take cli input
        char input[100];
        if (read_line(input, sizeof(input)) < 0) {
            break;
        }

        // saves input (command) to history
        history_list[nhistory].order = nhistory + 1;
        snprintf(history_list[nhistory].command, sizeof(history_list[nhistory].command), "%s", input);
        nhistory++;

        // tokenization step (breaking input into arguments array)
        char *args[64];
        int nargs = 0;
        char token[1024];
        int len = 0;
        bool in_token = false;
        bool in_squote = false;
        bool in_dquote = false;

        char *allocated[64]; // kept to free args addresses after reassignment like >
        int nalloc = 0;
        // saving the terminal fd to restore after
        int saved_stdout = -1;
        int saved_stderr = -1;

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
                        args[nargs] = strdup(token);
                        allocated[nalloc++] = args[nargs++];
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
            args[nargs] = strdup(token);
            allocated[nalloc++] = args[nargs++];
        }

        args[nargs] = NULL; // null-terminates args array

        // a trailing & means run job in background
        bool background_job = false;
        if (nargs > 0 && strcmp(args[nargs - 1], "&") == 0) {
            background_job = true;
            args[--nargs] = NULL;
        }


        // pipeline detection block
        char **stages[16];
        int stage_argc[16]; // argument count for each stage
        int nstages = 0;
        int start = 0;

        for (int i = 0; i < nargs; i++) {
            if (strcmp(args[i], "|") == 0) {
                args[i] = NULL;     // NULL terminates to make the array before it one complete stage
                stages[nstages] = &args[start];     // since args is just the address to the frist element of the array which is a string pointer (char *), so the first element in the stages array is a pointer type to the first string pointer in the args array
                stage_argc[nstages] = i - start;    // calculates how many argumetns were in this stage
                nstages++;
                start = i + 1;  // repositions start to the element after '|'
                // similarly, for future stages, they will be pointers to the address of the string pointer element right after the new start index => &(*(args + start))
            }
        }

        // appends the last stage after the last '|'
        stages[nstages] = &args[start];
        stage_argc[nstages] = nargs - start;
        nstages++;
        
        int prev_read = -1;  // the fd for the read end left over from the previous stage
        
        
        pid_t pids[16];

        if (nstages > 1) {
            for (int s = 0; s < nstages; s++) {
                bool last_stage = (s == nstages - 1);
                int fd[2];

                if (!last_stage && pipe(fd) == -1) {
                    perror("pipe error");
                    break;
                }

                pid_t pid = fork();

                if (pid == 0) {
                    // on the first stage, it's stdin is just the terminal so we don't set it's read end which is pointing to stage 2
                    if (prev_read != -1) {
                        // after the first stage, we set the stdin for the current stage to be the last stage's read fd or prev_read
                        dup2(prev_read, 0);
                        close(prev_read);
                    }

                    if (!last_stage) {
                        // if it's not the last stage, then we set the write end of the current pipe to be this stage
                        dup2(fd[1], 1);
                        close(fd[0]);
                        close(fd[1]);
                    }

                    run_stage(stages[s], stage_argc[s]);
                }

                pids[s] = pid; // stores the child pid into the array

                if (prev_read != -1) {
                    close(prev_read); // parent doesn't need it
                }
                
                if (!last_stage) {
                    close(fd[1]);
                    prev_read = fd[0];  // carry the read end forward
                }
            }

            for (int s = 0; s < nstages; s++) {
                waitpid(pids[s], NULL, 0);
            }

            goto free_args;
        }

        // if no args inputed, free args memory and loop
        if (args[0] == NULL) {
            goto free_args;
        }

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

        // redirect stdout to the file for builtins
        if (out_filename != NULL && is_builtin(args[0])) {
            int fd = open(out_filename, O_WRONLY | O_CREAT | (out_append ? O_APPEND : O_TRUNC), 0644);

            if (fd < 0) {
                perror(out_filename);
                goto free_args;
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
                goto free_args;
            }

            saved_stderr = dup(2);
            dup2(fd, 2);
            close(fd);
        }

        // exits the loop
        if (strcmp(args[0], "exit") == 0) {
            break;
        }

        // runs any built-in commands
        else if (is_builtin(args[0])) {
            run_builtin(args, nargs);
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
                    // child process, becomes the program

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

                    // executes the executable. Any form of the exec() function replaces the process that called it's memory, and the child process now is the exec() process,
                    // but the process still has the same PID, parent, and file descriptors so the waitpid() call in the parent still waits for this process to finish.
                    execv(full_path, args);

                    // only runs if execv failed
                    perror("execv");
                    exit(1);
                }

                else if (pid > 0) {
                    if (background_job) {
                        if (njobs < 64) {
                            // stores the current job's data into the jobs array
                            int max = 0;
                            for (int i = 0; i < njobs; i++) {
                                if (jobs[i].number > max) {
                                    max = jobs[i].number;
                                }
                            }
                            jobs[njobs].number = max + 1;
                            jobs[njobs].pid = pid;
                            int len = strlen(input);
                            // the below bloack is for getting rid of the spaces and & at the end of the input since the format for printing a Done job ommits it
                            while (len > 0 && (input[len-1] == ' ' || input[len-1] == '\t')) {
                                len--;
                            } // strips trailing spaces until '&'
                            if (len > 0 && input[len-1] == '&') {
                                len--;
                            } // gets rid of '&'
                            while (len > 0 && (input[len-1] == ' ' || input[len-1] == '\t')) {
                                len--;
                            } // gets rid of spaces between '&' and the actual end of the command
                            snprintf(jobs[njobs].command, sizeof(jobs[njobs].command), "%.*s", len, input);
                        }
                        printf("[%d] %d\n", jobs[njobs].number, pid); //prints the job number and the child process identifier
                        njobs++;
                    }

                    else {
                        // parent process waits for the child to finish
                        int status;
                        waitpid(pid, &status, 0);
                    }
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

        free_args:
            for (int i = 0; i < nalloc; i++) {
                free(allocated[i]);
            }
    }

    // writes command to history file on exit
    if (histfile) {
        save_history(histfile, "a", last_appended);
    }

    return 0;
}