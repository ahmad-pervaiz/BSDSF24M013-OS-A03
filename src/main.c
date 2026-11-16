#include "shell.h"

int main() {
    char* cmdline;

    init_readline();

    while ((cmdline = readline(PROMPT)) != NULL) {
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
        execute_pipeline(preprocessed);
        free(preprocessed);

        free(cmdline);
    }

    clear_history();
    printf("\nShell exited.\n");
    return 0;
}
