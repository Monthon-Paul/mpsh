/*
 * mpsh - My own Simple Shell program
 * has the following to exec programs,
 * I/O redirect, Piping & many more
 *
 * @author Monthon Paul
 * @version February 4, 2024
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MPSH_TOK_BUFSIZE 32
#define MPSH_HISTORY_CMD 200
#define MPSH_TOK_DELIM " \t\r\n\a"

/* I/O redirections */
typedef struct IO {
    char **input;
    char **output;
} IO;

static IO files;
static unsigned short piping, bg, jobs, history;

/* List of builtin commands */
static char *builtin_str[] = {"cd", "help", "history", "quit"};

/* forward declarations */
char ***mpsh_split_line(char *line);
int mpsh_execute(char ***args, char **cmds);
int mpsh_launch(char ***args, char **cmds);
int mpsh_piping(char ***args);
int mpsh_history(char **cmds);
void mpsh_redirect(int pos);
int mpsh_cd(char **args);
int mpsh_help(char **args);
int mpsh_exit(char **args);
char *mpsh_read_line();
int mpsh_size_builtins();
void mpsh_loop();

int (*builtin_func[])(char **) = {
    &mpsh_cd,
    &mpsh_help,
    &mpsh_history,
    &mpsh_exit};

/**
 * @brief Main entry point.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return status code
 */
int main(int argc, char **argv) {
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
    char *cmds[MPSH_HISTORY_CMD] = {NULL};
    history = jobs = 0;

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
    for (int i = 0; i < bufsize; i++)
        tokens[i] = calloc(bufsize, sizeof(char *));
    char *token, **tokens_backup;
    piping = bg = 0;

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
            bg = 1;
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
    if (*args[1] && piping)
        return mpsh_piping(args);
    for (int i = 0; i < mpsh_size_builtins(); i++) {
        if(!strcmp(**args, "history") && !strcmp(**args, builtin_str[i]))
            return (*builtin_func[i])(cmds);
        else if (!strcmp(**args, builtin_str[i]))
            return (*builtin_func[i])(*args);
    }

    return mpsh_launch(args, cmds);  // launch
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
int mpsh_launch(char ***args, char **cmds) {
    pid_t pid;
    int status;
    /* loop through listing */
    for (int i = 0; *args[i] != NULL; i++) {
        if ((pid = fork()) == 0) {
            mpsh_redirect(i);
            if (execvp(*args[i], args[i]) == -1) {
                printf("%s: Command not found\n", *args[i]);
                exit(EXIT_FAILURE);
            }
        }
        /* Parent waits for child to terminate, unless it's background */
        if (!bg)
            waitpid(pid, &status, 0);
        else
            printf("[%d] (%d) %s", ++jobs, pid, cmds[history - 1]);
    }
    return 1;
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
