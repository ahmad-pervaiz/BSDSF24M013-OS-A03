#include "shell.h"

int execute(char* arglist[]) {
    int status;
    int cpid = fork();

    switch (cpid) {
        case -1:
            perror("fork failed");
            return 1;
        case 0: // Child process
            execvp(arglist[0], arglist);
            perror("Command not found");
            exit(127);
        default: // Parent process
            waitpid(cpid, &status, 0);
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                return 128 + WTERMSIG(status);
            }
            return 1;
    }
}

int setup_redirection(char** arglist, char** exec_args) {
    int arg_idx = 0;
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], "<") == 0) {
            if (arglist[i + 1] == NULL) {
                fprintf(stderr, "syntax error near unexpected token 'newline'\n");
                return -1;
            }
            int in_fd = open(arglist[i + 1], O_RDONLY);
            if (in_fd < 0) {
                perror(arglist[i + 1]);
                return -1;
            }
            if (dup2(in_fd, STDIN_FILENO) < 0) {
                perror("dup2 stdin");
                close(in_fd);
                return -1;
            }
            close(in_fd);
            i++; // skip filename
        } else if (strcmp(arglist[i], ">") == 0) {
            if (arglist[i + 1] == NULL) {
                fprintf(stderr, "syntax error near unexpected token 'newline'\n");
                return -1;
            }
            int out_fd = open(arglist[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (out_fd < 0) {
                perror(arglist[i + 1]);
                return -1;
            }
            if (dup2(out_fd, STDOUT_FILENO) < 0) {
                perror("dup2 stdout");
                close(out_fd);
                return -1;
            }
            close(out_fd);
            i++; // skip filename
        } else {
            exec_args[arg_idx++] = arglist[i];
        }
    }
    exec_args[arg_idx] = NULL;
    return 0;
}

static int split_pipeline(char* cmdline, char* commands[], int max_cmds) {
    int count = 0;
    int in_single = 0, in_double = 0;
    char* start = cmdline;

    for (int i = 0; cmdline[i] != '\0'; i++) {
        if (cmdline[i] == '\'' && !in_double) in_single = !in_single;
        else if (cmdline[i] == '\"' && !in_single) in_double = !in_double;
        else if (cmdline[i] == '|' && !in_single && !in_double) {
            cmdline[i] = '\0';
            if (count < max_cmds) {
                commands[count++] = start;
            }
            start = &cmdline[i + 1];
        }
    }
    if (count < max_cmds) {
        commands[count++] = start;
    }
    return count;
}

int execute_pipeline(char* cmdline, int is_background) {
    char* commands[MAXARGS];
    int num_cmds = split_pipeline(cmdline, commands, MAXARGS);

    if (num_cmds == 0) {
        return 0;
    }

    if (num_cmds == 1) {
        char** arglist = tokenize(commands[0]);
        if (arglist == NULL || arglist[0] == NULL) {
            free_arglist(arglist);
            return 0;
        }

        int exit_code = 0;

        // Check if there is redirection
        int has_redir = 0;
        for (int i = 0; arglist[i] != NULL; i++) {
            if (strcmp(arglist[i], "<") == 0 || strcmp(arglist[i], ">") == 0) {
                has_redir = 1;
                break;
            }
        }

        if (!has_redir) {
            if (!is_background && (strcmp(arglist[0], "cd") == 0 || strcmp(arglist[0], "exit") == 0)) {
                exit_code = handle_builtin(arglist) ? 0 : 1;
            } else if (!is_background && handle_builtin(arglist) == 1) {
                exit_code = 0;
            } else {
                pid_t cpid = fork();
                if (cpid < 0) {
                    perror("fork failed");
                    exit_code = 1;
                } else if (cpid == 0) {
                    if (handle_builtin(arglist) == 1) {
                        exit(0);
                    }
                    execvp(arglist[0], arglist);
                    perror("Command not found");
                    exit(127);
                } else {
                    if (is_background) {
                        add_job(cpid, cmdline);
                        printf("[%d] %d\n", get_job_id(cpid), cpid);
                        exit_code = 0;
                    } else {
                        int status;
                        waitpid(cpid, &status, 0);
                        if (WIFEXITED(status)) {
                            exit_code = WEXITSTATUS(status);
                        } else if (WIFSIGNALED(status)) {
                            exit_code = 128 + WTERMSIG(status);
                        } else {
                            exit_code = 1;
                        }
                    }
                }
            }
        } else {
            if (!is_background && (strcmp(arglist[0], "cd") == 0 || strcmp(arglist[0], "exit") == 0)) {
                exit_code = handle_builtin(arglist) ? 0 : 1;
            } else {
                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork failed");
                    exit_code = 1;
                } else if (pid == 0) {
                    char* exec_args[MAXARGS + 1];
                    if (setup_redirection(arglist, exec_args) < 0) {
                        exit(1);
                    }
                    if (exec_args[0] == NULL) {
                        exit(0);
                    }
                    if (handle_builtin(exec_args) == 0) {
                        execvp(exec_args[0], exec_args);
                        perror("Command not found");
                        exit(127);
                    }
                    exit(0);
                } else {
                    if (is_background) {
                        add_job(pid, cmdline);
                        printf("[%d] %d\n", get_job_id(pid), pid);
                        exit_code = 0;
                    } else {
                        int status;
                        waitpid(pid, &status, 0);
                        if (WIFEXITED(status)) {
                            exit_code = WEXITSTATUS(status);
                        } else if (WIFSIGNALED(status)) {
                            exit_code = 128 + WTERMSIG(status);
                        } else {
                            exit_code = 1;
                        }
                    }
                }
            }
        }

        free_arglist(arglist);
        return exit_code;
    }

    // num_cmds > 1: Pipeline
    int pipefds[num_cmds - 1][2];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds[i]) < 0) {
            perror("pipe");
            return -1;
        }
    }

    pid_t pids[num_cmds];
    for (int i = 0; i < num_cmds; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            return -1;
        }
        if (pids[i] == 0) {
            if (i > 0) {
                if (dup2(pipefds[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(1);
                }
            }
            if (i < num_cmds - 1) {
                if (dup2(pipefds[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(1);
                }
            }
            for (int k = 0; k < num_cmds - 1; k++) {
                close(pipefds[k][0]);
                close(pipefds[k][1]);
            }

            char** sub_args = tokenize(commands[i]);
            if (sub_args == NULL || sub_args[0] == NULL) {
                exit(0);
            }
            char* exec_args[MAXARGS + 1];
            if (setup_redirection(sub_args, exec_args) < 0) {
                exit(1);
            }
            if (exec_args[0] == NULL) {
                exit(0);
            }
            if (handle_builtin(exec_args) == 0) {
                execvp(exec_args[0], exec_args);
                perror("Command not found");
                exit(127);
            }
            exit(0);
        }
    }

    for (int k = 0; k < num_cmds - 1; k++) {
        close(pipefds[k][0]);
        close(pipefds[k][1]);
    }

    if (is_background) {
        add_job(pids[num_cmds - 1], cmdline);
        printf("[%d] %d\n", get_job_id(pids[num_cmds - 1]), pids[num_cmds - 1]);
        return 0;
    } else {
        int last_status = 0;
        for (int i = 0; i < num_cmds; i++) {
            int status;
            waitpid(pids[i], &status, 0);
            if (i == num_cmds - 1) {
                last_status = status;
            }
        }
        if (WIFEXITED(last_status)) {
            return WEXITSTATUS(last_status);
        } else if (WIFSIGNALED(last_status)) {
            return 128 + WTERMSIG(last_status);
        }
        return 1;
    }
}
