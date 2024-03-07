#ifndef MPSH_H
#define MPSH_H

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define MPSH_TOK_BUFSIZE 32
#define MPSH_CMDS 200
#define MPSH_TOK_DELIM " \t\r\n\a"
#define MPSH_MAXJOBS 16
#define MPSH_MAXJID 1 << 16 /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

typedef struct job_t {              /* The job struct */
    pid_t pid;                      /* job PID */
    int jid;                        /* job ID [1, 2, ...] */
    int state;                      /* UNDEF, BG, FG, or ST */
    char cmdline[MPSH_TOK_BUFSIZE]; /* command line */
} job_t;

job_t jobs[MPSH_MAXJOBS]; /* The job list */

/* I/O redirections */
typedef struct IO {
    char **input;
    char **output;
} IO;

/* List of builtin commands */
static char *builtin_str[] = {"help", "quit", "cd", "history", "jobs", "fg", "bg"};

int (*builtin_func[])(char **) = {
    &mpsh_help,
    &mpsh_exit,
    &mpsh_cd,
    &mpsh_history};

/* forward declarations */
char ***mpsh_split_line(char *line);
int mpsh_execute(char ***args, char **cmds);
int mpsh_launch(char ***args, sigset_t sigs);
int mpsh_piping(char ***args);
int mpsh_history(char **cmds);
int mpsh_cd(char **args);
int mpsh_help(char **args);
int mpsh_exit(char **args);
void mpsh_redirect(int pos);
void mpsh_bg(int jid);
void mpsh_fg(int jid);
void waitfg(pid_t pid);
char *concatstr(char **str, int bg);
char *mpsh_read_line();
int mpsh_size_builtins();
void mpsh_loop();

void sigchld_handler(int sig);
// void sigtstp_handler(int sig);
// void sigint_handler(int sig);
// void sigquit_handler(int sig);

void clearjob(job_t *job);
void initjobs(job_t *jobs);
int maxjid(job_t *jobs);
int addjob(job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(job_t *jobs, pid_t pid);
job_t *getjobpid(job_t *jobs, pid_t pid);
job_t *getjobjid(job_t *jobs, int jid);
int pid2jid(pid_t pid);
int listjobs(job_t *jobs);

void unix_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

#endif  // include guard