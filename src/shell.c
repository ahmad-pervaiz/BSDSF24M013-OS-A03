#include "shell.h"

char* preprocess_operators(const char* line) {
    if (line == NULL) return NULL;
    int len = strlen(line);
    char* result = (char*)malloc(len * 3 + 1);
    int j = 0;
    int in_single = 0, in_double = 0;

    for (int i = 0; line[i] != '\0'; i++) {
        char c = line[i];
        if (c == '\'' && !in_double) {
            in_single = !in_single;
            result[j++] = c;
        } else if (c == '\"' && !in_single) {
            in_double = !in_double;
            result[j++] = c;
        } else if (!in_single && !in_double && (c == '<' || c == '>' || c == '|' || c == ';' || c == '&')) {
            result[j++] = ' ';
            result[j++] = c;
            result[j++] = ' ';
        } else {
            result[j++] = c;
        }
    }
    result[j] = '\0';
    return result;
}

int split_by_semicolon(char* line, char* commands[], int max_cmds) {
    int count = 0;
    int in_single = 0, in_double = 0;
    char* start = line;

    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] == '\'' && !in_double) in_single = !in_single;
        else if (line[i] == '\"' && !in_single) in_double = !in_double;
        else if (line[i] == ';' && !in_single && !in_double) {
            line[i] = '\0';
            if (count < max_cmds) {
                commands[count++] = start;
            }
            start = &line[i + 1];
        }
    }
    if (count < max_cmds) {
        commands[count++] = start;
    }
    return count;
}

int check_background(char* cmd) {
    int len = strlen(cmd);
    while (len > 0 && (cmd[len - 1] == ' ' || cmd[len - 1] == '\t' || cmd[len - 1] == '\n')) {
        cmd[--len] = '\0';
    }
    if (len > 0 && cmd[len - 1] == '&') {
        cmd[--len] = '\0';
        while (len > 0 && (cmd[len - 1] == ' ' || cmd[len - 1] == '\t')) {
            cmd[--len] = '\0';
        }
        return 1;
    }
    return 0;
}

void free_arglist(char** arglist) {
    if (arglist == NULL) return;
    for (int i = 0; arglist[i] != NULL; i++) {
        free(arglist[i]);
    }
    free(arglist);
}

char** tokenize(char* cmdline) {
    // Edge case: empty command line
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n') {
        return NULL;
    }

    char** arglist = (char**)malloc(sizeof(char*) * (MAXARGS + 1));
    for (int i = 0; i < MAXARGS + 1; i++) {
        arglist[i] = (char*)malloc(sizeof(char) * ARGLEN);
        bzero(arglist[i], ARGLEN);
    }

    char* cp = cmdline;
    char* start;
    int len;
    int argnum = 0;

    while (*cp != '\0' && argnum < MAXARGS) {
        while (*cp == ' ' || *cp == '\t') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        if (*cp == '\"' || *cp == '\'') {
            char quote = *cp++;
            start = cp;
            len = 0;
            while (*cp != '\0' && *cp != quote) {
                len++;
                cp++;
            }
            if (*cp == quote) cp++;
        } else {
            start = cp;
            len = 1;
            while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
                len++;
            }
        }

        if (len >= ARGLEN) len = ARGLEN - 1;
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for (int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
}

// Background Job tracking
typedef struct {
    int job_id;
    pid_t pid;
    char command[MAX_LEN];
} Job;

static Job active_jobs[MAX_JOBS];
static int num_active_jobs = 0;
static int next_job_id = 1;

void add_job(pid_t pid, const char* cmd) {
    if (num_active_jobs >= MAX_JOBS) return;
    active_jobs[num_active_jobs].job_id = next_job_id++;
    active_jobs[num_active_jobs].pid = pid;

    // Skip leading whitespace in command
    while (*cmd == ' ' || *cmd == '\t') cmd++;

    strncpy(active_jobs[num_active_jobs].command, cmd, MAX_LEN - 1);
    active_jobs[num_active_jobs].command[MAX_LEN - 1] = '\0';
    num_active_jobs++;
}

void remove_job(pid_t pid) {
    int found = -1;
    for (int i = 0; i < num_active_jobs; i++) {
        if (active_jobs[i].pid == pid) {
            found = i;
            break;
        }
    }
    if (found != -1) {
        for (int i = found; i < num_active_jobs - 1; i++) {
            active_jobs[i] = active_jobs[i + 1];
        }
        num_active_jobs--;
        if (num_active_jobs == 0) {
            next_job_id = 1;
        }
    }
}

void check_jobs(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_job(pid);
    }
}

int print_jobs(void) {
    check_jobs();
    for (int i = 0; i < num_active_jobs; i++) {
        printf("[%d]  %d  Running  %s\n",
               active_jobs[i].job_id,
               active_jobs[i].pid,
               active_jobs[i].command);
    }
    return 1;
}

int get_job_id(pid_t pid) {
    for (int i = 0; i < num_active_jobs; i++) {
        if (active_jobs[i].pid == pid) {
            return active_jobs[i].job_id;
        }
    }
    return 1;
}

int handle_builtin(char** arglist) {
    if (arglist == NULL || arglist[0] == NULL) {
        return 0;
    }

    // 1. exit: Terminates the shell gracefully
    if (strcmp(arglist[0], "exit") == 0) {
        int status = 0;
        if (arglist[1] != NULL) {
            status = atoi(arglist[1]);
        }
        clear_history();
        exit(status);
    }

    // 2. cd <directory>: Changes the current working directory
    if (strcmp(arglist[0], "cd") == 0) {
        char* dir = arglist[1];
        if (dir == NULL) {
            dir = getenv("HOME");
        }
        if (dir == NULL) {
            fprintf(stderr, "cd: expected argument\n");
        } else if (chdir(dir) != 0) {
            perror("cd");
        }
        return 1;
    }

    // 3. help: Displays a brief help message listing built-in commands
    if (strcmp(arglist[0], "help") == 0) {
        printf("FCIT Custom Shell - Built-in Commands:\n");
        printf("  cd <directory>  : Change the current working directory\n");
        printf("  exit            : Terminate the shell\n");
        printf("  help            : Display this help message\n");
        printf("  history         : Display command history\n");
        printf("  jobs            : List active background jobs\n");
        return 1;
    }

    // 4. history: Displays command history
    if (strcmp(arglist[0], "history") == 0) {
        print_history();
        return 1;
    }

    // 5. jobs: Lists active background jobs
    if (strcmp(arglist[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }

    return 0; // Not a built-in command
}

// Built-in command generator for Readline tab completion
static const char* builtins[] = {
    "cd", "exit", "help", "history", "jobs", NULL
};

char* builtin_generator(const char* text, int state) {
    static int list_index, len;
    char* name;

    if (!state) {
        list_index = 0;
        len = strlen(text);
    }

    while ((name = (char*)builtins[list_index++])) {
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }

    return NULL;
}

char** shell_completion(const char* text, int start, int end) {
    char** matches = NULL;

    // Only complete built-in commands at the start of the command line
    if (start == 0) {
        matches = rl_completion_matches(text, builtin_generator);
    }

    // If matches is NULL, Readline automatically falls back to default filename completion
    return matches;
}

void init_readline(void) {
    using_history();
    stifle_history(HISTORY_SIZE);
    rl_attempted_completion_function = shell_completion;
}

char* get_history(int n) {
    HIST_ENTRY **the_list = history_list();
    if (the_list == NULL || n < 1) {
        return NULL;
    }
    int count = 0;
    while (the_list[count] != NULL) {
        count++;
    }
    if (n > count) {
        return NULL;
    }
    return the_list[n - 1]->line;
}

int print_history(void) {
    HIST_ENTRY **the_list = history_list();
    if (the_list != NULL) {
        for (int i = 0; the_list[i] != NULL; i++) {
            printf("%d  %s\n", i + 1, the_list[i]->line);
        }
    }
    return 1;
}
