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
#define PROMPT "FCIT> "

// Function prototypes
char** tokenize(char* cmdline);
void free_arglist(char** arglist);
char* preprocess_operators(const char* line);
int execute(char** arglist);
int execute_pipeline(char* cmdline);
int setup_redirection(char** arglist, char** exec_args);
int handle_builtin(char** arglist);

// Readline and history helper prototypes
void init_readline(void);
char** shell_completion(const char* text, int start, int end);
char* builtin_generator(const char* text, int state);
char* get_history(int n);
int print_history(void);

#endif // SHELL_H
