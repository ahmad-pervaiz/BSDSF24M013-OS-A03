#include "shell.h"

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

        start = cp;
        len = 1;
        while (*++cp != '\0' && !(*cp == ' ' || *cp == '\t')) {
            len++;
        }
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';
        argnum++;
    }

    if (argnum == 0) { // No arguments were parsed
        for(int i = 0; i < MAXARGS + 1; i++) free(arglist[i]);
        free(arglist);
        return NULL;
    }

    arglist[argnum] = NULL;
    return arglist;
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

    // 5. jobs: Placeholder for Feature 6
    if (strcmp(arglist[0], "jobs") == 0) {
        printf("Job control not yet implemented.\n");
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
