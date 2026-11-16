#include "shell.h"

int main() {
    char* cmdline;

    init_readline();

    while (1) {
        // Reap zombie background processes before showing prompt
        check_jobs();

        cmdline = readline(PROMPT);
        if (cmdline == NULL) {
            break;
        }

        // Also reap any background processes that completed while waiting
        check_jobs();

        // Skip leading whitespace to check if input is empty
        char* trimmed = cmdline;
        while (*trimmed == ' ' || *trimmed == '\t') {
            trimmed++;
        }

        if (*trimmed == '\0') {
            free(cmdline);
            continue;
        }

        // Handle !n re-execution before adding to history or tokenizing
        if (*trimmed == '!') {
            char* num_str = trimmed + 1;
            while (*num_str == ' ' || *num_str == '\t') {
                num_str++;
            }

            if (*num_str == '\0') {
                fprintf(stderr, "myshell: !: event not found\n");
                free(cmdline);
                continue;
            }

            char* endptr;
            long n = strtol(num_str, &endptr, 10);
            while (*endptr == ' ' || *endptr == '\t') {
                endptr++;
            }

            if (endptr == num_str || *endptr != '\0') {
                fprintf(stderr, "myshell: !%s: event not found\n", num_str);
                free(cmdline);
                continue;
            }

            char* target = get_history((int)n);
            if (target == NULL) {
                fprintf(stderr, "myshell: !%ld: event not found\n", n);
                free(cmdline);
                continue;
            }

            // Print the expanded command
            printf("%s\n", target);

            // Replace cmdline with the retrieved command
            free(cmdline);
            cmdline = strdup(target);
        }

        // Add the non-empty command to Readline history
        add_history(cmdline);

        // Preprocess line to ensure proper spacing around special operators
        char* preprocessed = preprocess_operators(cmdline);

        // Split by semicolon for command chaining (;)
        char* chained_commands[MAXARGS];
        int num_chained = split_by_semicolon(preprocessed, chained_commands, MAXARGS);

        for (int i = 0; i < num_chained; i++) {
            char* subcmd = chained_commands[i];
            while (*subcmd == ' ' || *subcmd == '\t') {
                subcmd++;
            }
            if (*subcmd == '\0') {
                continue;
            }

            // Check if this command should run in background (&)
            int is_bg = check_background(subcmd);

            // Execute command / pipeline
            execute_pipeline(subcmd, is_bg);
        }

        free(preprocessed);
        free(cmdline);
    }

    clear_history();
    printf("\nShell exited.\n");
    return 0;
}
