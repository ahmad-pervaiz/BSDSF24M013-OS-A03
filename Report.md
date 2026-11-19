# Operating Systems (CS-311) - Programming Assignment 03
# Comprehensive Technical Report & Viva Defense Guide

**Student Name:** Ahmad Pervaiz  
**Roll Number:** BSDSF24M013  
**Course:** Operating Systems (BS Data Science, 5th Semester)  
**Institution:** Punjab University College of Information Technology (PUCIT), University of the Punjab  
**Instructor:** Muhammad Arif Butt, PhD  
**Repository:** [github.com/ahmad-pervaiz/BSDSF24M013-OS-A03](https://github.com/ahmad-pervaiz/BSDSF24M013-OS-A03)  
**Target Shell Binary:** `bin/myshell`

---

## Table of Contents
1. [Executive Summary & Architectural Overview](#1-executive-summary--architectural-overview)
2. [Process Lifecycle & The Fork-Exec-Wait Cycle](#2-process-lifecycle--the-fork-exec-wait-cycle)
3. [Feature-by-Feature Implementation & Verification Guide](#3-feature-by-feature-implementation--verification-guide)
   - [Feature 1: The Starting Point (Base Shell - v1.0-base)](#feature-1-the-starting-point-base-shell---v10-base)
   - [Feature 2: Built-in Commands (Tag v2)](#feature-2-built-in-commands-tag-v2)
   - [Feature 3: Command History (Tag v3)](#feature-3-command-history-tag-v3)
   - [Feature 4: Tab Completion with GNU Readline (Tag v4)](#feature-4-tab-completion-with-gnu-readline-tag-v4)
   - [Feature 5: I/O Redirection and Pipes (Tag v5)](#feature-5-io-redirection-and-pipes-tag-v5)
   - [Feature 6: Command Chaining and Background Execution (Tag v6)](#feature-6-command-chaining-and-background-execution-tag-v6)
   - [Feature 7: The if-then-else-fi Control Structure (Tag v7)](#feature-7-the-if-then-else-fi-control-structure-tag-v7)
   - [Feature 8: Shell Variables & Environment (Tag v8)](#feature-8-shell-variables--environment-tag-v8)
   - [Feature 9: Collaborative Git Workflow](#feature-9-collaborative-git-workflow)
4. [In-Depth Viva-Voce Questions and Answers](#4-in-depth-viva-voce-questions-and-answers)
5. [Testing & Demonstration Matrix](#5-testing--demonstration-matrix)
6. [Grading Rubric Compliance Table](#6-grading-rubric-compliance-table)

---

## 1. Executive Summary & Architectural Overview

The objective of this assignment is to develop a fully functional UNIX command-line interpreter (`myshell`) from foundational operating system primitives. The implementation follows professional systems engineering practices, employing modular C architecture, strict POSIX compliance, robust memory management, and a rigorous Git branch-merge-tag-release development cycle.

### Modular Project Organization
The project source code is organized into a clean, decoupled structure:
```
BSDSF24M013-OS-A03/
├── include/
│   └── shell.h          # Header declarations, constants, prototypes, struct definitions
├── src/
│   ├── main.c           # Read-Eval-Print Loop (REPL), prompt management, control dispatcher
│   ├── shell.c          # Tokenization, Readline integration, built-ins, jobs, variables, if-parser
│   └── execute.c        # Process control (fork-exec-wait), I/O redirection, pipeline execution
├── obj/                 # Compiled relocatable object files (*.o)
├── bin/                 # Final executable binary (bin/myshell)
├── Makefile             # Automated build script with dependency tracking
├── README.md            # Repository documentation
├── Report.md            # Comprehensive assignment and technical viva report
└── Report.pdf           # Formatted PDF version of the report
```

### Architectural Principles
1. **Separation of Concerns:** `main.c` manages user interaction and high-level control; `shell.c` encapsulates lexical parsing, symbol expansion, and stateful tracking; `execute.c` manages low-level kernel system calls (`fork`, `execvp`, `waitpid`, `pipe`, `dup2`, `open`, `close`).
2. **Defensive Resource Management:** Dynamic heap allocations are paired with rigorous deallocation routines (`free_arglist`, `free_variables`, `clear_history`). Open file descriptors are strictly closed across both parent and child processes.
3. **Standards Compliance:** Built with `gcc -Wall -g -Iinclude`, linking GNU Readline (`-lreadline`), generating zero compiler warnings.

---

## 2. Process Lifecycle & The Fork-Exec-Wait Cycle

### The Classic UNIX Process Model
A command-line interpreter acts as an interface between the user and the kernel. The core operating system mechanism underpinning command execution is the **fork-exec-wait** cycle:

```
[ START: Display Prompt & Wait ]
              │
              ▼
    [ Read Command Line ]
              │
              ▼
    [ Parse & Tokenize ]
              │
        Empty line? ────► Yes ───► [ Loop Back ]
              │ No
      Built-in command? ──► Yes ──► [ Execute in Shell Process ] ──► [ Return to Prompt ]
              │ No
              ▼
   [ fork() System Call ]
              │
     ┌────────┴────────┐
     ▼                 ▼
[ Child Process ]  [ Parent Process ]
 (PID == 0)         (PID > 0)
     │                 │
 [ Setup I/O ]         │
 [ Redirections ]      │
 [ Pipes ]             │
     │                 │
 [ execvp() ]          │
  Replaces image       │
  with binary          ▼
     │            Is Background (&)?
  Executes             ├───► Yes ──► [ Add to Jobs List, Print PID, Return to Prompt ]
  command              └───► No  ──► [ waitpid(PID, &status, 0) ]
     │                                    │
 [ Exit with status ] ◄───────────────────┘
```

### Detailed Execution Phase Breakdown
1. **Process Creation (`fork`):** The parent shell clones itself. The kernel creates an exact duplicate virtual memory address space, copying page tables, open file descriptor tables, and process context. The return value differentiates the child (`PID == 0`) from the parent (`PID == child_pid`).
2. **File Descriptor Manipulation (`dup2` & `close`):** If I/O redirection (`<`, `>`) or pipes (`|`) are present, the child redirects standard streams (`STDIN_FILENO = 0`, `STDOUT_FILENO = 1`) to the appropriate file or pipe end, closing intermediate descriptors.
3. **Image Replacement (`execvp`):** The child invokes `execvp(arglist[0], arglist)`. The kernel replaces the child's virtual address space, stack, heap, and code segment with the executable found in the `PATH`.
4. **Parent Synchronization (`waitpid`):**
   - **Foreground commands:** The parent blocks on `waitpid(cpid, &status, 0)`. Upon child termination, the kernel reaps the child's process table entry and returns the termination status. The macro `WEXITSTATUS(status)` extracts the exit code (0 for success, 1–255 for failure).
   - **Background commands (`&`):** The parent skips `waitpid()`, records the child PID in the active jobs registry, and immediately presents the next prompt.

---

## 3. Feature-by-Feature Implementation & Verification Guide

### Feature 1: The Starting Point (Base Shell - v1.0-base)
- **Concepts Covered:** Project modularization, header file encapsulation, compilation pipelines with `make`, base fork-exec-wait cycle, Git initialization.
- **Implementation:**
  - Configured project directory structure with `include/`, `src/`, and `Makefile`.
  - Implemented base command reading (`read_cmd`), basic whitespace splitting (`tokenize`), and child execution (`execute`).
- **How to Test:**
  ```bash
  make clean && make
  ./bin/myshell
  FCIT> ls -l
  FCIT> pwd
  FCIT> whoami
  FCIT> [Press Ctrl+D to exit]
  ```
- **Git Metadata:** Tag `v1.0-base`, Release 1 published.

---

### Feature 2: Built-in Commands (Tag v2)
- **Concepts Covered:** Shell vs. Child process execution context, Process state mutation (`chdir`), System call error reporting (`perror`).
- **Why Built-ins Are Necessary:**
  Commands like `cd` cannot be external programs. When an external program runs, it runs in a child process; changing the working directory of a child has no effect on the parent shell. Therefore, `cd` must invoke `chdir()` directly within the parent shell process. Similarly, `exit` must terminate the parent shell process itself.
- **Implementation Details:**
  Implemented `int handle_builtin(char** arglist)` in `src/shell.c`:
  - `exit`: Invokes `exit(0)`.
  - `cd <dir>`: Validates arguments, defaults to `getenv("HOME")` if empty, executes `chdir(dir)`, and reports failures via `perror("cd")`.
  - `help`: Displays a list of all implemented built-in commands.
  - `jobs`: Placeholder displaying `"Job control not yet implemented."` (fully implemented in Feature 6).
- **How to Test:**
  ```bash
  ./bin/myshell
  FCIT> help
  FCIT> cd ..
  FCIT> pwd          # Confirms directory moved up
  FCIT> cd -         # Or test nonexistent path
  FCIT> cd nonexistent_dir_xyz
  # Expected: cd: No such file or directory
  FCIT> jobs
  FCIT> exit
  ```
- **Git Metadata:** Branch `feature-built-ins`, Tag `v2`, Release 2 published.

---

### Feature 3: Command History (Tag v3)
- **Concepts Covered:** Dynamic string manipulation, circular FIFO buffers, history expansion (`!n`), bounds validation.
- **Implementation Details:**
  - Configured `HISTORY_SIZE 20`. Implemented FIFO array storage in `src/shell.c`:
    - `add_history()`: Stores copies of non-empty commands. When full, shifts entries left (`history[i-1] = history[i]`), freeing the oldest entry.
    - `get_history(int n)`: Retrieves 1-based $n$-th command.
    - `print_history()`: Formats history with line numbers (`1  echo test`).
    - `clear_history()`: Frees all allocated strings upon termination.
  - Intercepted `!n` syntax in `src/main.c` before tokenization: parses integer $n$, checks bounds, echoes the retrieved command, updates `cmdline`, logs into history, and executes.
- **How to Test:**
  ```bash
  ./bin/myshell
  FCIT> echo first
  FCIT> echo second
  FCIT> pwd
  FCIT> history
  # Shows entries 1 through 4
  FCIT> !2
  # Echoes: echo second, executes second
  FCIT> !99
  # Expected stderr: myshell: !99: event not found
  FCIT> exit
  ```
- **Git Metadata:** Branch `feature-history`, Tag `v3`, Release 3 published.

---

### Feature 4: Tab Completion with GNU Readline (Tag v4)
- **Concepts Covered:** Third-party library linking (`-lreadline`), terminal line-editing API, autocompletion generator callbacks.
- **Implementation Details:**
  - Added `LDFLAGS = -lreadline` to `Makefile`.
  - Replaced `read_cmd()` with `readline(PROMPT)`.
  - Initialized history with `using_history()` and `stifle_history(HISTORY_SIZE)`.
  - Integrated Readline's history with `add_history()` and `history_list()`, enabling native <kbd>↑</kbd> and <kbd>↓</kbd> arrow key history navigation.
  - Implemented custom tab autocompletion via `rl_attempted_completion_function`:
    - At `start == 0`, `builtin_generator` autocompletes built-ins (`cd`, `exit`, `help`, `history`, `jobs`, `set`).
    - At `start > 0` or on unrecognized prefixes, falls back to Readline's default filename/path completer.
- **How to Test:**
  ```bash
  ./bin/myshell
  FCIT> hel[TAB]         # Auto-completes to "help "
  FCIT> cat Make[TAB]    # Auto-completes to "Makefile "
  FCIT> [Press UP ARROW] # Cycles to previous command
  FCIT> exit
  ```
- **Git Metadata:** Branch `feature-readline`, Tag `v4`, Release 4 published.

---

### Feature 5: I/O Redirection and Pipes (Tag v5)
- **Concepts Covered:** File descriptors (`0 = STDIN`, `1 = STDOUT`), Kernel system calls (`open`, `close`, `dup2`, `pipe`), pipeline synchronization.
- **Implementation Details:**
  - **Operator Preprocessing & Lexing:** Enhanced parser with `preprocess_operators()` to pad `<`, `>`, `|` with whitespace while preserving quoted substrings (`"..."`).
  - **Redirection Logic (`setup_redirection` in `src/execute.c`):**
    - `> file`: Opens target with `open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644)`. Duplicates descriptor with `dup2(fd, STDOUT_FILENO)` and closes `fd`.
    - `< file`: Opens source with `open(file, O_RDONLY)`. Duplicates descriptor with `dup2(fd, STDIN_FILENO)` and closes `fd`.
  - **Pipeline Logic (`execute_pipeline` in `src/execute.c`):**
    - Supports arbitrary multi-stage pipelines ($N$ commands, $N-1$ pipes).
    - Creates pipes with `pipe(pipefds[k])`.
    - Forks $N$ child processes: child $i$ binds its stdin to `pipefds[i-1][0]` (if $i > 0$) and its stdout to `pipefds[i][1]` (if $i < N-1$).
    - **Deadlock Prevention:** Crucially closes all pipe ends in both the parent and children to ensure EOF is properly received by downstream readers.
    - Parent waits for pipeline children to terminate.
- **How to Test:**
  ```bash
  ./bin/myshell
  FCIT> ls -l > file_list.txt
  FCIT> head -n 3 < file_list.txt
  FCIT> cat /etc/passwd | grep "root"
  FCIT> sort < file_list.txt > sorted.txt
  FCIT> cat sorted.txt | grep Makefile | wc -l
  FCIT> rm -f file_list.txt sorted.txt
  FCIT> exit
  ```
- **Git Metadata:** Branch `feature-io-redirection`, Tag `v5`, Release 5 published.

---

### Feature 6: Command Chaining and Background Execution (Tag v6)
- **Concepts Covered:** Asynchronous multitasking, Job control tables, Zombie process lifecycle, Non-blocking process reaping (`waitpid` with `WNOHANG`).
- **Implementation Details:**
  - **Command Chaining (`;`):** Implemented `split_by_semicolon()`. Iterates over semicolon-separated commands, executing each sequentially.
  - **Background Execution (`&`):** Implemented `check_background()` to detect and strip trailing `&`. When detected:
    - Child process is forked.
    - Parent registers the process in `Job active_jobs[MAX_JOBS]` storing `job_id`, `pid`, and `command`.
    - Parent displays `[job_id] PID` and returns immediately without blocking.
  - **Zombie Reaping (`check_jobs`):**
    - Before displaying the prompt, the shell runs a non-blocking reap loop:
      ```c
      while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
          remove_job(pid);
      }
      ```
    - Removes finished PIDs from the active jobs table.
  - **Built-in `jobs` Command:** Iterates through `active_jobs` and prints all currently executing background jobs.
- **How to Test:**
  ```bash
  ./bin/myshell
  FCIT> echo "First" ; sleep 1 ; echo "Second"
  # Verifies sequential execution
  FCIT> sleep 5 &
  [1] 12345
  FCIT> jobs
  [1]  12345  Running  sleep 5
  # Wait 5 seconds, press Enter:
  FCIT> jobs
  # Process reaped; jobs table is now empty
  FCIT> exit
  ```
- **Git Metadata:** Branch `feature-multitasking`, Tag `v6`, Release 6 published.

---

### Feature 7: The if-then-else-fi Control Structure (Tag v7)
- **Concepts Covered:** Scripting language control flow, multi-line state machines, shell subshell prompts, command exit status decoding.
- **Implementation Details:**
  - Created `handle_if_statement()` in `src/shell.c`.
  - When input begins with `if`, enters multi-line capture state. Reads lines with secondary prompt `> ` until matching `fi` is encountered.
  - Separates commands into `then_cmds` and optional `else_cmds`.
  - Executes the condition command via `execute_pipeline()`.
  - Uses `WIFEXITED(status)` and `WEXITSTATUS(status)` to retrieve the condition's exit code:
    - Exit status `0` (Success) $\rightarrow$ Executes commands in `then` block.
    - Exit status $\neq 0$ (Failure) $\rightarrow$ Executes commands in `else` block (if defined).
- **How to Test:**
  ```bash
  ./bin/myshell
  FCIT> if grep "root" /etc/passwd > /dev/null
  > then
  > echo "Found root user"
  > else
  > echo "Root user missing"
  > fi
  # Output: Found root user

  FCIT> if [ -f "Makefile" ]
  > then
  > echo "Makefile exists."
  > fi
  # Output: Makefile exists.

  FCIT> if false
  > then
  > echo "Should not print"
  > else
  > echo "Condition failed successfully"
  > fi
  # Output: Condition failed successfully
  FCIT> exit
  ```
- **Git Metadata:** Branch `feature-if-then-else`, Tag `v7`, Release 7 published.

---

### Feature 8: Shell Variables & Environment (Tag v8)
- **Concepts Covered:** Dynamic symbol tables, variable assignment parsing, token expansion, quote handling.
- **Implementation Details:**
  - **Storage:** Implemented a linked-list symbol table (`ShellVar`) with `set_variable()`, `get_variable()`, and `free_variables()`.
  - **Assignment Detection (`VARNAME=value`):**
    - Detected via `is_assignment()`. Executed as a built-in action without forking.
    - Supports values with spaces (`MESSAGE="Hello, World!"`).
  - **Variable Expansion (`$VARNAME`):**
    - `expand_arglist()` and `expand_variables()` inspect tokens prior to command execution.
    - Replaces `$VARNAME` with its stored value (falls back to `getenv()` or empty string).
    - `strip_quotes()` strips surrounding quotes after expansion.
  - **Built-in `set` Command:** Prints all currently defined shell variables (`print_variables()`).
  - **Documentation:** Updated `help` built-in and Readline completion table to include `set`.
- **How to Test:**
  ```bash
  ./bin/myshell
  FCIT> MSG="hello"
  FCIT> set
  # Displays: MSG=hello
  FCIT> echo $MSG
  # Output: hello
  FCIT> MSG="world"
  FCIT> echo $MSG
  # Output: world
  FCIT> MESSAGE="Hello, Systems Programming!"
  FCIT> echo $MESSAGE
  # Output: Hello, Systems Programming!
  FCIT> help
  # Lists: set
  FCIT> exit
  ```
- **Git Metadata:** Branch `feature-variables`, Tag `v8`, Release 8 published.

---

### Feature 9: Collaborative Git Workflow
- **Concepts Covered:** Open-source collaboration, Forking, Upstream tracking, Issue reporting, Feature branching, Pull Requests, Code Review, and Merging.
- **Workflow Simulation:**
  1. A secondary GitHub account forks the primary repository (`github.com/ahmad-pervaiz/BSDSF24M013-OS-A03`).
  2. An issue is filed: *"Bug: 'help' command documentation discrepancy"*.
  3. A fix branch `fix-help-documentation` is created in the fork, addressing the issue.
  4. A Pull Request is opened against `main` of the primary repository.
  5. As project maintainer, the pull request is reviewed, validated against tests, and merged into `main`.
  6. Local workspace is synchronized using `git pull origin main`.

---

## 4. In-Depth Viva-Voce Questions and Answers

### Q1: Why must commands like `cd` and `exit` be built into the shell instead of implemented as separate executable binaries?
**Answer:**  
In UNIX, every process possesses its own isolated virtual address space and private process control block (PCB), which includes its current working directory (CWD) and process termination state. When an external executable runs, the shell invokes `fork()`, creating a child process. If `cd` were an external binary, the child process would change its own working directory via `chdir()` and immediately terminate, leaving the parent shell's working directory completely unchanged. Similarly, if `exit` were an external program, the child process would exit, but the parent shell would remain active waiting for input. Therefore, `cd` and `exit` must execute within the execution context of the shell process itself.

---

### Q2: What exact steps occur in the kernel during a `fork()`-`execvp()`-`waitpid()` sequence?
**Answer:**  
1. **`fork()`:** The kernel duplicates the calling process. It allocates a new PCB and PID, duplicates the memory descriptors (using Copy-on-Write / COW on modern Linux), and copies the file descriptor table so both parent and child share open file descriptions. The child receives return value `0`, while the parent receives the child's `PID`.
2. **`execvp()`:** Invoked by the child. The kernel deallocates the child's existing memory spaces (code, heap, stack), reads the ELF binary from disk, loads the text and data segments, initializes a clean user stack containing `argc`, `argv`, and `envp`, and sets the Instruction Pointer (EIP/RIP) to the binary's entry point (`_start`). The child's PID and open file descriptors (unless flagged `FD_CLOEXEC`) are preserved.
3. **`waitpid()`:** The parent suspends execution. The scheduler transitions the parent from the `RUNNING` state to `INTERRUPTIBLE_SLEEP` until the child changes state (terminates). Upon child exit, the kernel sends a `SIGCHLD` signal to the parent, awakens it, returns the child's termination status, and frees the child's PCB.

---

### Q3: Why is closing unused pipe file descriptors in both parent and child processes mandatory? What happens if you forget?
**Answer:**  
A pipe has a read end (`pipefds[0]`) and a write end (`pipefds[1]`). A reader reading from a pipe only receives End-of-File (`EOF`, return value `0`) when **all** open file descriptors pointing to the write end of the pipe across all processes in the system have been closed.
- If the parent fails to close `pipefds[1]`, the write descriptor remains open in the parent.
- When the upstream child (the writer) terminates, the downstream child (the reader, e.g., `grep` or `wc`) waits for more data indefinitely because the kernel sees that the parent process still holds an open write reference.
- As a consequence, the reader will **hang forever (deadlock)**, and the shell will freeze. Closing all unused pipe ends in the parent and children ensures clean EOF propagation.

---

### Q4: What is a zombie process, why does it occur, and how does `waitpid()` with `WNOHANG` solve it?
**Answer:**  
- **Definition:** A zombie (defunct) process is a child process that has completed execution via `exit()`, but whose exit status has not yet been collected ("reaped") by its parent process using `wait()` or `waitpid()`.
- **Operating System Implication:** Although the child's memory is released, the kernel must retain its PID, exit code, and accounting structures in the process table until the parent collects it. If background processes are spawned without reaping, the process table eventually exhausts available PIDs, preventing any new process creation.
- **Solution (`WNOHANG`):** Standard `waitpid(pid, &status, 0)` blocks until the process finishes. Passing `WNOHANG` (`waitpid(-1, &status, WNOHANG)`) tells the kernel to check if any child has exited: if a terminated child exists, it reaps it and returns its PID immediately; if no child has exited, it returns `0` immediately without blocking. Calling this in a loop before displaying the prompt guarantees non-blocking cleanup of background jobs.

---

### Q5: How does I/O redirection work using `dup2()`, and what happens to the original file descriptors?
**Answer:**  
`dup2(oldfd, newfd)` atomically duplicates the open file description pointed to by `oldfd` onto `newfd`. If `newfd` was open, the kernel quietly closes it first.
- In output redirection (`> file`): The child calls `out_fd = open("file", O_WRONLY | O_CREAT | O_TRUNC, 0644)`, followed by `dup2(out_fd, STDOUT_FILENO)`. Now file descriptor 1 points to `file`. Any subsequent `printf()` or `write(1, ...)` issued by `execvp` writes directly to `file`.
- In input redirection (`< file`): The child calls `in_fd = open("file", O_RDONLY)`, followed by `dup2(in_fd, STDIN_FILENO)`. Now file descriptor 0 reads from `file`.
- After calling `dup2()`, the child calls `close(out_fd)` or `close(in_fd)`. Closing the original descriptor does not affect `STDIN_FILENO` or `STDOUT_FILENO` because the kernel maintains an internal reference counter on the underlying file description.

---

### Q6: How does GNU Readline integrate into the shell, and how does custom tab autocompletion work?
**Answer:**  
GNU Readline takes control of terminal line discipline using `termios`, enabling raw mode to intercept keypresses (such as <kbd>Tab</kbd>, <kbd>↑</kbd>, <kbd>↓</kbd>, <kbd>Backspace</kbd>).
- Readline maintains an internal history buffer initialized with `using_history()` and bounded by `stifle_history()`.
- Autocompletion is implemented by assigning a callback to `rl_attempted_completion_function`:
  ```c
  rl_attempted_completion_function = shell_completion;
  ```
- When the user presses <kbd>Tab</kbd>, Readline invokes `shell_completion(text, start, end)`.
- If `start == 0` (the first token on the line), the shell checks against built-in command names using `rl_completion_matches(text, builtin_generator)`.
- If `start > 0` or if no built-in matches, `shell_completion` returns `NULL`. Readline automatically falls back to its default filename completion function (`rl_filename_completion_function`), inspecting files and directories in the current working directory.

---

### Q7: How does variable expansion work in `myshell` without invoking external binaries?
**Answer:**  
Variable management is handled directly in memory within `src/shell.c`:
1. Variable assignments (`VARNAME=value`) are detected by `is_assignment()` before execution and stored in a linked-list symbol table (`ShellVar`) mapping names to values.
2. Prior to dispatching arguments to `execvp()`, the shell invokes `expand_arglist()`. It iterates through each string in `arglist`.
3. If an argument contains `$`, `expand_variables()` parses the variable name following `$`, looks it up in the symbol table (or system environment via `getenv()`), and substitutes it into a newly allocated string buffer.
4. The unexpanded token is freed and replaced with the expanded string. Outer quotes are stripped by `strip_quotes()`. This ensures arguments like `echo $MSG` receive the exact variable contents without launching subshells.

---

### Q8: How does the `if-then-else-fi` state machine parse multi-line input and evaluate exit codes?
**Answer:**  
When the shell detects `if` at the beginning of a line:
1. It enters a dedicated multi-line parsing state machine inside `handle_if_statement()`.
2. The condition expression following `if` is stored in a condition buffer.
3. The shell repeatedly calls `readline("> ")` to prompt for subsequent lines until `fi` is reached. Lines encountered after `then` are appended to `then_cmds[]`; lines encountered after `else` are appended to `else_cmds[]`.
4. Once `fi` is read, the shell executes the condition command via `execute_pipeline()`.
5. The exit status is collected via `waitpid(cpid, &status, 0)` and inspected using `WEXITSTATUS(status)`.
6. If the exit code is `0` (POSIX standard for truth/success), the shell sequentially executes each command in `then_cmds[]`. If the exit code is non-zero, it executes each command in `else_cmds[]`.

---

## 5. Testing & Demonstration Matrix

| Feature | Test Input Command | Expected Terminal Behavior / Output |
|---|---|---|
| **Base Execution** | `ls -l ; whoami` | Displays file details and current username |
| **Built-in `cd`** | `cd .. && pwd` | Current directory changes to parent |
| **Built-in `help`** | `help` | Lists `cd`, `exit`, `help`, `history`, `jobs`, `set` |
| **Built-in `exit`** | `exit` | Closes `myshell` cleanly with exit code 0 |
| **History Logging** | `history` | Displays numbered history of last 20 commands |
| **History `!n`** | `!2` | Echoes command #2 and executes it |
| **Tab Completion** | `hel[TAB]` / `cat Make[TAB]` | Autocompletes built-ins and filenames |
| **Output Redir (`>`)** | `ls -l > out.txt` | Output redirected to `out.txt`; nothing printed to terminal |
| **Input Redir (`<`)** | `head -n 2 < out.txt` | Reads input from `out.txt` and prints first 2 lines |
| **Pipes (`\|`)** | `cat /etc/passwd \| grep root` | Piped output filtered cleanly without hanging |
| **Chaining (`;`)** | `echo A ; sleep 1 ; echo B` | Prints A, waits 1 second, then prints B sequentially |
| **Background (`&`)** | `sleep 5 &` | Prints `[1] PID`, immediately shows prompt |
| **Jobs Tracking** | `jobs` | Lists `[1]  PID  Running  sleep 5` |
| **Zombie Reaping** | *Wait 5s, press Enter*, `jobs` | Process reaped; `jobs` shows empty list |
| **`if-then-else-fi`** | `if true\nthen\necho YES\nfi` | Evaluates condition exit status 0, prints `YES` |
| **Variable Assign** | `MSG="hello world"` | Stored in symbol table |
| **Variable `set`** | `set` | Prints `MSG=hello world` |
| **Variable Expand** | `echo $MSG` | Expands and prints `hello world` |

---

## 6. Grading Rubric Compliance Table

| Feature ID | Feature Description | Git Tag | Max Marks | Status | Implementation Verification |
|---|---|---|---|---|---|
| **Feature 1** | The Starting Point (Base Shell) | `v1.0-base` | 05 | **Complete** | Modular starter code, Makefile, base REPL |
| **Feature 2** | Built-in Commands (`cd`, `exit`, `help`, `jobs`) | `v2` | 10 | **Complete** | Handled in parent shell; `chdir()`, `exit()` |
| **Feature 3** | Command History (`history`, `!n`) | `v3` | 10 | **Complete** | FIFO buffer (20 items), `!n` expansion |
| **Feature 4** | Tab Completion with Readline | `v4` | 15 | **Complete** | `-lreadline`, arrow key navigation, completion |
| **Feature 5** | I/O Redirection & Pipes (`<`, `>`, `\|`) | `v5` | 15 | **Complete** | `dup2`, `pipe`, `open`, multi-stage pipelines |
| **Feature 6** | Chaining (`;`) & Background (`&`) | `v6` | 15 | **Complete** | Sequential chains, `WNOHANG` zombie reaping, `jobs` |
| **Feature 7** | `if-then-else-fi` Control Structure | `v7` | 10 | **Complete** | Multi-line parsing, `WEXITSTATUS` conditional branches |
| **Feature 8** | Shell Variables (`VAR=val`, `$VAR`, `set`) | `v8` | 10 | **Complete** | Linked list symbol table, `$VAR` expansion, `set` |
| **Feature 9** | Git Workflow (Fork, Issues, PRs, Merge) | - | 10 | **Complete** | Collaborative simulation, branch history |
| **Total** | **Comprehensive Shell Project** | | **100** | **100 / 100** | **Ready for Viva-Voce Oral Defense** |

---
*Report prepared by Ahmad Pervaiz (BSDSF24M013) for Operating Systems Assignment 03.*
