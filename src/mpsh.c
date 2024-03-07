/*
 * mpsh - My own Simple Shell program
 * has the following to exec programs,
 * I/O redirect, Piping & many more
 *
 * @author Monthon Paul
 * @version March 7, 2024
 */
#include "mpsh.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global variables */
static IO files;
static unsigned short piping, history;
static int *bg;
static char concat[MPSH_CMDS];
int nextjid = 1; /* next job ID to allocate */

/**
 * @brief Main entry point.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return status code
 */
int main(int argc, char **argv) {
    /* Install the signal handlers */

    // Signal(SIGINT, sigint_handler);   /* ctrl-c */
    // Signal(SIGTSTP, sigtstp_handler); /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler); /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    // Signal(SIGQUIT, sigquit_handler);

    /* Initialize the job list */
    initjobs(jobs);

    // Run command loop.
    mpsh_loop();
    return 0;
}

/**
 * @brief Loop getting input and executing it.
 */
void mpsh_loop() {
    char *line;
    char ***args;
    int status;
    char *cmds[MPSH_CMDS] = {NULL};
    history = 0;

    do {
        printf("mpsh$ ");
        line = mpsh_read_line();
        if (strcmp(line, "\n"))
            cmds[history++] = strdup(line);
        args = mpsh_split_line(line);
        status = mpsh_execute(args, cmds);

        for (int i = 0; i < MPSH_TOK_BUFSIZE; i++)
            free(args[i]);
        free(files.input);
        free(files.output);
        free(line);
        free(args);
        free(bg);
    } while (status);
}

/**
 * @brief Read a line of input from stdin.
 * @return The line from stdin.
 */
char *mpsh_read_line() {
    char *line = NULL;
    size_t bufsize = 0;  // have getline allocate a buffer for us

    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);  // We recieved an EOF
        } else {
            perror("mpsh: readline");
            exit(EXIT_FAILURE);
        }
    }
    return line;
}
/**
 * @brief Split a line into tokens (very naively).
 * @param line The line.
 * @return Null-terminated array of tokens.
 */
char ***mpsh_split_line(char *line) {
    int bufsize = MPSH_TOK_BUFSIZE, pos = 0, cmdpos = 0;
    char ***tokens = calloc(bufsize, sizeof(char *));
    files.input = calloc(bufsize, sizeof(char *));
    files.output = calloc(bufsize, sizeof(char *));
    bg = malloc(bufsize * sizeof(int));
    for (int i = 0; i < bufsize; i++)
        tokens[i] = calloc(bufsize, sizeof(char *));
    char *token, **tokens_backup;
    piping = 0;

    if (!tokens) {
        fprintf(stderr, "mpsh: allocation error\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, MPSH_TOK_DELIM);
    while (token != NULL) {
        if (!strcmp(token, "<")) {
            token = strtok(NULL, MPSH_TOK_DELIM);
            files.input[cmdpos] = token;
        } else if (!strcmp(token, ">")) {
            token = strtok(NULL, MPSH_TOK_DELIM);
            files.output[cmdpos] = token;
        } else if (!strcmp(token, "|")) {
            tokens[cmdpos][pos] = NULL;
            pos = 0, piping = 1;
            token = strtok(NULL, MPSH_TOK_DELIM);
            tokens[++cmdpos][pos++] = token;
        } else if (!strcmp(token, ";")) {
            tokens[cmdpos][pos] = NULL;
            pos = 0;
            token = strtok(NULL, MPSH_TOK_DELIM);
            tokens[++cmdpos][pos++] = token;
        } else if (!strcmp(token, "&")) {
            bg[cmdpos] = 1;
        } else {
            tokens[cmdpos][pos++] = token;
        }

        if (pos >= bufsize) {
            bufsize += MPSH_TOK_BUFSIZE;
            tokens_backup = tokens[cmdpos];
            tokens[cmdpos] = realloc(tokens, bufsize * sizeof(char *));
            if (!tokens) {
                free(tokens_backup);
                fprintf(stderr, "mpsh: allocation error\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, MPSH_TOK_DELIM);
    }
    tokens[cmdpos][pos] = NULL;
    return tokens;
}
/**
 * Get the size of builtin commands
 */
int mpsh_size_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

/**
 *  @brief Execute shell built-in or launch program.
 *  @param args Null terminated list of arguments.
 *  @return 1 if the shell should continue running, 0 if it should terminate
 */
int mpsh_execute(char ***args, char **cmds) {
    if (**args == NULL)
        return 1;  // An empty command was entered.

    // Signal Bockers
    sigset_t sigs;
    sigemptyset(&sigs);
    sigaddset(&sigs, SIGCHLD);
    sigaddset(&sigs, SIGSTOP);
    sigaddset(&sigs, SIGINT);

    if (*args[1] && piping)
        return mpsh_piping(args);
    for (int i = 0; i < mpsh_size_builtins(); i++) {
        if (!strcmp(**args, "jobs"))
            return listjobs(jobs);
        else if (!strcmp(**args, "history") && !strcmp(**args, builtin_str[i]))
            return (*builtin_func[i])(cmds);
        else if (!strcmp(**args, builtin_str[i]))
            return (*builtin_func[i])(*args);
    }

    return mpsh_launch(args, sigs);  // launch
}

/**
 * Help information about the Shell program
 * @param args list of arguments (including program).
 * @return 1 if the shell should continue running, 0 if it should terminate
 */
int mpsh_help(char **args) {
    printf("Monthon Paul MPSH\n");
    printf("Type program names and arguments, and hit enter.\n");
    printf("The following are built in:\n");

    for (int i = 0; i < mpsh_size_builtins(); i++)
        printf("  %s\n", builtin_str[i]);

    printf("Use the man command for information on other programs.\n");
    return 1;
}

/**
 * quit out of the Shell program
 * @param args list of arguments (including program).
 */
int mpsh_exit(char **args) {
    exit(EXIT_SUCCESS);
}

/**
 * @brief show history of commands enter
 * @param cmds list of commands enter
 * @return Always returns 1, to continue execution.
 */
int mpsh_history(char **cmds) {
    for (int i = 0; cmds[i] != NULL; i++)
        printf("%d %s", i + 1, cmds[i]);
    return 1;
}

/**
 * @brief exec change directory
 * @param args list of arguments (including program).
 * @return Always returns 1, to continue execution.
 */
int mpsh_cd(char **args) {
    if (args[1]) {
        if (chdir(args[1]) == -1)
            perror("mpsh");
    } else {
        printf("expected argument for cd\n");
    }
    return 1;
}

/**
 * @brief Launch a program and wait for it to terminate.
 * @param args Null terminated list of arguments (including program).
 * @return Always returns 1, to continue execution.
 */
int mpsh_launch(char ***args, sigset_t sigs) {
    pid_t pid;

    /* loop through listing */
    for (int i = 0; *args[i] != NULL; i++) {
        sigprocmask(SIG_BLOCK, &sigs, NULL);
        if ((pid = fork()) == 0) {
            // Set process group ID
            setpgid(0, 0);

            // need to unblock before exec call
            sigprocmask(SIG_UNBLOCK, &sigs, NULL);
            mpsh_redirect(i);
            if (execvp(*args[i], args[i]) == -1) {
                printf("%s: Command not found\n", *args[i]);
                exit(EXIT_FAILURE);
            }
        }
        addjob(jobs, pid, (bg[i]) ? BG : FG, concatstr(args[i], bg[i]));
        sigprocmask(SIG_UNBLOCK, &sigs, NULL);

        /* Parent waits for child to terminate, unless it's background */
        if (!bg[i])
            waitfg(pid);
        else
            printf("[%d] (%d) %s", pid2jid(pid), pid, concatstr(args[i], bg[i]));
    }
    return 1;
}

/**
 *
 *
 */
char *concatstr(char **str, int bg) {
    *concat = '\0';
    for (int i = 0; str[i] != NULL; i++) {
        strcat(concat, str[i]);
        if (str[i + 1] != NULL)
            strcat(concat, " ");
    }
    if (bg)
        strcat(concat, " &");
    return strcat(concat, "\n");  // add newline at end
}

/*
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid) {
    job_t *fg_job = getjobpid(jobs, pid);
    // loop around sigsupend so the shell is blocked until fg is not null and still in FG state.
    while (fg_job != NULL && fg_job->state == FG) {
        // block signal for a bit and unblock
        sigset_t empty;
        sigemptyset(&empty);
        sigsuspend(&empty);
    }
}

/**
 * @brief I/O redirect to a file
 * @param pos I/O position for input file or output file
 */
void mpsh_redirect(int pos) {
    char *input = files.input[pos], *output = files.output[pos];
    if (input) {
        int in = open(input, O_RDONLY);
        if (in == -1) {
            perror("mpsh");
            exit(EXIT_FAILURE);
        }
        dup2(in, STDIN_FILENO);
        close(in);
    }
    if (output) {
        int out = creat(output, 0644);
        if (out == -1) {
            perror("mpsh");
            exit(EXIT_FAILURE);
        }
        dup2(out, STDOUT_FILENO);
        close(out);
    }
}

/**
 * @brief the shell Pipline cmd
 * @param args 2D array of strings terminated by NULL
 * @return Always returns 1, to continue execution.
 */
int mpsh_piping(char ***args) {
    pid_t pid;
    int fds[2], size, next_input;
    for (size = 0; *args[size] != NULL; size++) {
    }
    pipe(fds);
    if ((pid = fork()) == 0) {  // special case: redirect only to stdout
        // first child redirects write end to stdout
        dup2(fds[1], STDOUT_FILENO);

        // close the read & write
        close(fds[0]);
        close(fds[1]);

        // check for I/O redirects
        mpsh_redirect(0);
        if (execvp(**args, *args) == -1) {
            printf("%s: Command not found\n", **args);
            exit(1);
        }
    }
    /* middle pipeline loop */
    for (int i = 1; i < size - 1; i++) {
        close(fds[1]);
        next_input = fds[0];
        pipe(fds);
        if ((pid = fork()) == 0) {
            dup2(next_input, STDIN_FILENO);
            dup2(fds[1], STDOUT_FILENO);
            close(fds[0]);

            // check for I/O redirects
            mpsh_redirect(i);
            if (execvp(*args[i], args[i]) == -1) {
                printf("%s: Command not found\n", *args[i]);
                exit(1);
            }
        }
        close(next_input);
    }
    close(fds[1]);
    next_input = fds[0];
    if ((pid = fork()) == 0) {  // special case: redirect only to stdin
        // first child redirects read end to stdin
        dup2(next_input, STDIN_FILENO);

        // check for I/O redirects
        mpsh_redirect(size - 1);
        if (execvp(*args[size - 1], args[size - 1]) == -1) {
            printf("%s: Command not found\n", *args[size - 1]);
            exit(1);
        }
    }
    close(next_input);
    // Parent
    int status;
    for (int i = 0; i < size - 1; i++)
        waitpid(pid, &status, 0);
    return 1;
}

/*****************
 * Signal handlers
 *****************/

/*
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.
 */
void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    // Must reap potentially multiple children because signals are not queued.
    // Use WNOHANG or WUNTRACED so we can stop this loop as soon as no zombies are available.
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        if (WIFSIGNALED(status)) {
            printf("Job [%d] (%d) terminated by signal %d\n", pid2jid(pid), pid, SIGINT);
            deletejob(jobs, pid);
        } else if (WIFSTOPPED(status)) {
            printf("Job [%d] (%d) stopped by signal %d\n", pid2jid(pid), pid, SIGTSTP);
            getjobpid(jobs, pid)->state = ST;
        } else if (WIFEXITED(status))
            deletejob(jobs, pid);
    }
}

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(job_t *jobs) {
    for (int i = 0; i < MPSH_MAXJOBS; i++)
        clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(job_t *jobs) {
    int max = 0;
    for (int i = 0; i < MPSH_MAXJOBS; i++)
        if (jobs[i].jid > max)
            max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(job_t *jobs, pid_t pid, int state, char *cmdline) {
    if (pid < 1)
        return 0;
    for (int i = 0; i < MPSH_MAXJOBS; i++) {
        if (jobs[i].pid == 0) {
            jobs[i].pid = pid;
            jobs[i].state = state;
            jobs[i].jid = nextjid++;
            if (nextjid > MPSH_MAXJOBS)
                nextjid = 1;
            strcpy(jobs[i].cmdline, cmdline);
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(job_t *jobs, pid_t pid) {
    if (pid < 1)
        return 0;
    for (int i = 0; i < MPSH_MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs) + 1;
            return 1;
        }
    }
    return 0;
}

/* getjobpid - Find a job (by PID) on the job list */
job_t *getjobpid(job_t *jobs, pid_t pid) {
    if (pid < 1)
        return NULL;
    for (int i = 0; i < MPSH_MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return &jobs[i];
    return NULL;
}

/* getjobjid - Find a job (by JID) on the job list */
job_t *getjobjid(job_t *jobs, int jid) {
    if (jid < 1)
        return NULL;
    for (int i = 0; i < MPSH_MAXJOBS; i++)
        if (jobs[i].jid == jid)
            return &jobs[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) {
    if (pid < 1)
        return 0;
    for (int i = 0; i < MPSH_MAXJOBS; i++)
        if (jobs[i].pid == pid)
            return jobs[i].jid;
    return 0;
}

/* listjobs - Print the job list */
int listjobs(job_t *jobs) {
    for (int i = 0; i < MPSH_MAXJOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
            switch (jobs[i].state) {
                case BG:
                    printf("Running ");
                    break;
                case FG:
                    printf("Foreground ");
                    break;
                case ST:
                    printf("Stopped ");
                    break;
                default:
                    printf("listjobs: Internal error: job[%d].state=%d ", i, jobs[i].state);
            }
            printf("%s", jobs[i].cmdline);
        }
    }
    return 1;
}

/******************************
 * end job list helper routines
 ******************************/

/***********************
 * Other helper routines
 ***********************/

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg) {
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) {
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
        unix_error("Signal error");
    return (old_action.sa_handler);
}