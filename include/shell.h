#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_LEN 512
#define MAXARGS 32
#define ARGLEN 128
#define HISTORY_SIZE 20
#define MAX_JOBS 64
#define PROMPT "FCIT> "

// Function prototypes
char** tokenize(char* cmdline);
void free_arglist(char** arglist);
char* preprocess_operators(const char* line);
int split_by_semicolon(char* line, char* commands[], int max_cmds);
int check_background(char* cmd);
int execute(char** arglist);
int execute_pipeline(char* cmdline, int is_background);
int setup_redirection(char** arglist, char** exec_args);
int handle_builtin(char** arglist);
int handle_if_statement(char* first_line);

// Job control prototypes
void add_job(pid_t pid, const char* cmd);
void remove_job(pid_t pid);
void check_jobs(void);
int print_jobs(void);
int get_job_id(pid_t pid);

// Variable management prototypes
int set_variable(const char* name, const char* value);
const char* get_variable(const char* name);
int print_variables(void);
void free_variables(void);
int is_assignment(const char* s);
char* expand_variables(const char* str);
void expand_arglist(char** arglist);
int handle_assignment_statement(const char* stmt);

// Readline and history helper prototypes
void init_readline(void);
char** shell_completion(const char* text, int start, int end);
char* builtin_generator(const char* text, int state);
char* get_history(int n);
int print_history(void);

#endif // SHELL_H
