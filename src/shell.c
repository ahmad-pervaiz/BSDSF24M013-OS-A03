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
    if (cmdline == NULL || cmdline[0] == '\0' || cmdline[0] == '\n' || cmdline[0] == '\r') {
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
        while (*cp == ' ' || *cp == '\t' || *cp == '\r' || *cp == '\n') cp++; // Skip leading whitespace
        
        if (*cp == '\0') break; // Line was only whitespace

        start = cp;
        len = 0;
        int in_quote = 0;
        char quote_char = 0;

        while (*cp != '\0') {
            if (!in_quote && (*cp == '\"' || *cp == '\'')) {
                in_quote = 1;
                quote_char = *cp;
            } else if (in_quote && *cp == quote_char) {
                in_quote = 0;
            } else if (!in_quote && (*cp == ' ' || *cp == '\t' || *cp == '\r' || *cp == '\n')) {
                break;
            }
            len++;
            cp++;
        }

        if (len >= ARGLEN) len = ARGLEN - 1;
        strncpy(arglist[argnum], start, len);
        arglist[argnum][len] = '\0';

        // Strip matching outer quotes from token
        if (len >= 2 && ((arglist[argnum][0] == '\"' && arglist[argnum][len - 1] == '\"') ||
                         (arglist[argnum][0] == '\'' && arglist[argnum][len - 1] == '\''))) {
            memmove(arglist[argnum], arglist[argnum] + 1, len - 2);
            arglist[argnum][len - 2] = '\0';
        }

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

// Shell Variable Storage & Functions
typedef struct ShellVar {
    char* name;
    char* value;
    struct ShellVar* next;
} ShellVar;

static ShellVar* var_head = NULL;

int set_variable(const char* name, const char* value) {
    if (name == NULL || value == NULL) return -1;

    ShellVar* curr = var_head;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            free(curr->value);
            curr->value = strdup(value);
            return 0;
        }
        curr = curr->next;
    }

    ShellVar* node = (ShellVar*)malloc(sizeof(ShellVar));
    node->name = strdup(name);
    node->value = strdup(value);
    node->next = var_head;
    var_head = node;
    return 0;
}

const char* get_variable(const char* name) {
    if (name == NULL) return "";
    ShellVar* curr = var_head;
    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            return curr->value;
        }
        curr = curr->next;
    }
    char* env = getenv(name);
    return env ? env : "";
}

int print_variables(void) {
    ShellVar* curr = var_head;
    while (curr != NULL) {
        printf("%s=%s\n", curr->name, curr->value);
        curr = curr->next;
    }
    return 1;
}

void free_variables(void) {
    ShellVar* curr = var_head;
    while (curr != NULL) {
        ShellVar* next = curr->next;
        free(curr->name);
        free(curr->value);
        free(curr);
        curr = next;
    }
    var_head = NULL;
}

int is_assignment(const char* s) {
    if (s == NULL || !((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z') || s[0] == '_')) {
        return 0;
    }
    const char* eq = strchr(s, '=');
    if (eq == NULL) return 0;
    for (const char* p = s; p < eq; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) {
            return 0;
        }
    }
    return 1;
}

char* expand_variables(const char* str) {
    if (str == NULL) return NULL;
    char buffer[MAX_LEN * 2];
    int buf_idx = 0;

    for (int i = 0; str[i] != '\0' && buf_idx < sizeof(buffer) - 1; ) {
        if (str[i] == '$') {
            i++; // skip '$'
            char var_name[64];
            int v_idx = 0;
            while (str[i] != '\0' && (
                   (str[i] >= 'a' && str[i] <= 'z') ||
                   (str[i] >= 'A' && str[i] <= 'Z') ||
                   (str[i] >= '0' && str[i] <= '9') ||
                   str[i] == '_') && v_idx < 63) {
                var_name[v_idx++] = str[i++];
            }
            var_name[v_idx] = '\0';
            const char* val = get_variable(var_name);
            for (int k = 0; val[k] != '\0' && buf_idx < sizeof(buffer) - 1; k++) {
                buffer[buf_idx++] = val[k];
            }
        } else {
            buffer[buf_idx++] = str[i++];
        }
    }
    buffer[buf_idx] = '\0';
    return strdup(buffer);
}

void strip_quotes(char* str) {
    if (str == NULL) return;
    int len = strlen(str);
    if (len >= 2 && ((str[0] == '\"' && str[len - 1] == '\"') || (str[0] == '\'' && str[len - 1] == '\''))) {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

void expand_arglist(char** arglist) {
    if (arglist == NULL) return;
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strchr(arglist[i], '$') != NULL) {
            char* expanded = expand_variables(arglist[i]);
            free(arglist[i]);
            arglist[i] = expanded;
        }
        strip_quotes(arglist[i]);
    }
}

int handle_assignment_statement(const char* stmt) {
    char name[128];
    const char* eq = strchr(stmt, '=');
    if (eq == NULL) return 0;

    int nlen = eq - stmt;
    if (nlen >= sizeof(name)) nlen = sizeof(name) - 1;
    strncpy(name, stmt, nlen);
    name[nlen] = '\0';

    const char* val_start = eq + 1;
    char clean_val[MAX_LEN];
    int vlen = strlen(val_start);

    // Strip quotes if any
    if (vlen >= 2 && ((val_start[0] == '\"' && val_start[vlen - 1] == '\"') ||
                      (val_start[0] == '\'' && val_start[vlen - 1] == '\''))) {
        strncpy(clean_val, val_start + 1, vlen - 2);
        clean_val[vlen - 2] = '\0';
    } else {
        strncpy(clean_val, val_start, sizeof(clean_val) - 1);
        clean_val[sizeof(clean_val) - 1] = '\0';
    }

    // Expand any variables in the value
    char* expanded_val = expand_variables(clean_val);
    set_variable(name, expanded_val);
    free(expanded_val);
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
        free_variables();
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
    // 3. help: Displays a brief help message listing built-in commands
    if (strcmp(arglist[0], "help") == 0) {
        printf("FCIT Custom Shell - Built-in Commands:\n");
        printf("  cd <directory>  : Change the current working directory\n");
        printf("  exit            : Terminate the shell\n");
        printf("  help            : Display this help message\n");
        printf("  history         : Display command history\n");
        printf("  !n              : Re-execute the n-th command from history\n");
        printf("  jobs            : List active background jobs\n");
        printf("  set             : Display all shell variables\n");
        printf("  VAR=value       : Assign a shell variable\n");
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

    // 6. set: Displays all shell variables
    if (strcmp(arglist[0], "set") == 0) {
        print_variables();
        return 1;
    }

    return 0; // Not a built-in command
}

// Built-in command generator for Readline tab completion
static const char* builtins[] = {
    "cd", "exit", "help", "history", "jobs", "set", NULL
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

int handle_if_statement(char* first_line) {
    char* p = first_line;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    if (strncmp(p, "if", 2) != 0 || (p[2] != ' ' && p[2] != '\t' && p[2] != '\r' && p[2] != '\n' && p[2] != '\0')) {
        return 0; // Not an if statement
    }

    char condition[MAX_LEN] = "";
    char* then_cmds[MAXARGS];
    int num_then = 0;
    char* else_cmds[MAXARGS];
    int num_else = 0;

    char* cond_start = p + 2;
    while (*cond_start == ' ' || *cond_start == '\t' || *cond_start == '\r' || *cond_start == '\n') cond_start++;

    strncpy(condition, cond_start, MAX_LEN - 1);
    condition[MAX_LEN - 1] = '\0';
    int cl = strlen(condition);
    while (cl > 0 && (condition[cl - 1] == ' ' || condition[cl - 1] == '\t' || 
                      condition[cl - 1] == '\r' || condition[cl - 1] == '\n' || 
                      condition[cl - 1] == ';')) {
        condition[--cl] = '\0';
    }

    enum { STATE_NONE, STATE_THEN, STATE_ELSE } state = STATE_NONE;
    int fi_found = 0;

    while (!fi_found) {
        char* line = readline("> ");
        if (line == NULL) {
            fprintf(stderr, "syntax error: unexpected end of file while looking for matching 'fi'\n");
            break;
        }

        // Strip trailing \r, \n, spaces, and tabs
        int llen = strlen(line);
        while (llen > 0 && (line[llen - 1] == ' ' || line[llen - 1] == '\t' || 
                            line[llen - 1] == '\r' || line[llen - 1] == '\n')) {
            line[--llen] = '\0';
        }

        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t' || *trimmed == '\r' || *trimmed == '\n') trimmed++;

        if (*trimmed == '\0') {
            free(line);
            continue;
        }

        if (strcmp(trimmed, "then") == 0) {
            state = STATE_THEN;
            free(line);
        } else if (strcmp(trimmed, "else") == 0) {
            state = STATE_ELSE;
            free(line);
        } else if (strcmp(trimmed, "fi") == 0) {
            fi_found = 1;
            free(line);
            break;
        } else {
            if (state == STATE_THEN && num_then < MAXARGS) {
                then_cmds[num_then++] = line;
            } else if (state == STATE_ELSE && num_else < MAXARGS) {
                else_cmds[num_else++] = line;
            } else {
                free(line);
            }
        }
    }

    // Execute condition command
    int cond_exit = 1;
    if (strlen(condition) > 0) {
        char* prep = preprocess_operators(condition);
        cond_exit = execute_pipeline(prep, 0);
        free(prep);
    }

    // Based on exit code, execute the appropriate block
    if (cond_exit == 0) {
        for (int i = 0; i < num_then; i++) {
            char* prep = preprocess_operators(then_cmds[i]);
            char* chained[MAXARGS];
            int n_ch = split_by_semicolon(prep, chained, MAXARGS);
            for (int c = 0; c < n_ch; c++) {
                char* s = chained[c];
                while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
                int slen = strlen(s);
                while (slen > 0 && (s[slen - 1] == ' ' || s[slen - 1] == '\t' || 
                                    s[slen - 1] == '\r' || s[slen - 1] == '\n')) {
                    s[--slen] = '\0';
                }
                if (*s != '\0') {
                    int bg = check_background(s);
                    execute_pipeline(s, bg);
                }
            }
            free(prep);
        }
    } else {
        for (int i = 0; i < num_else; i++) {
            char* prep = preprocess_operators(else_cmds[i]);
            char* chained[MAXARGS];
            int n_ch = split_by_semicolon(prep, chained, MAXARGS);
            for (int c = 0; c < n_ch; c++) {
                char* s = chained[c];
                while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;
                int slen = strlen(s);
                while (slen > 0 && (s[slen - 1] == ' ' || s[slen - 1] == '\t' || 
                                    s[slen - 1] == '\r' || s[slen - 1] == '\n')) {
                    s[--slen] = '\0';
                }
                if (*s != '\0') {
                    int bg = check_background(s);
                    execute_pipeline(s, bg);
                }
            }
            free(prep);
        }
    }

    for (int i = 0; i < num_then; i++) free(then_cmds[i]);
    for (int i = 0; i < num_else; i++) free(else_cmds[i]);

    return 1;
}
