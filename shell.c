/*
 * shell.c
 *
 * Written by Drs. William Kreahling and Andy Dalton
 * Edited by Ethan Roland and Nick Sprinkle
 *
 * An implementation of a simple UNIX shell.  This program supports:
 *
 *     - Running processes
 *     - Redirecting standard output (>)
 *     - Redirecting standard input (<)
 *     - Appending standard output to a file (>>)
 *     - Redirecting both standard output and standard input (&>)
 *     - Creating process pipelines (p1 | p2 | ...)
 *     - Interrupting a running process (i.e., Ctrl-C)
 *     - A built-in version of the 'ls' command
 *     - A built-in version of the 'rm' command
 *
 * Among the many things it does _NOT_ support are:
 *
 *     - PATH searching -- you must supply the absolute path to all programs
 *       (e.g., /bin/ls instead of just ls)
 *     - Environment variables
 *     - Appending standard error to a file (2>>)
 *     - Appending both standard output and standard input (2&>)
 *     - Backgrounding processes (p1&)
 *     - Unconditionally chaining processes (p1;p2)
 *     - Conditionally chaining processes (p1 && p2 or p1 || p2)
 *     - Piping/IO redirection for built-in commands
 *
 * Keep in mind that this program was written to be easily understood/modified
 * for educational purposes.  The author makes no claim that this is the
 * "best" way to solve this problem.
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include "shellParser.h"

/* Macros to test whether a process ID is a parent's or a child's. */
#define PARENT_PID(pid) ((pid) > 0)
#define CHILD_PID(pid)  ((pid) == 0)


/* Function prototypes */
static char** promptAndRead(void);
static pid_t  forkWrapper(void);
static void   pipeWrapper(int fds[]);
static int    dupWrapper(int fd);
static bool   isSpecial(char* token);
static void   signalHandler();

static void   parseArgs(char** args, char** line, int* lineIndex);
static void   continueProcessingLine(char** line, int* lineIndex, char** args);
static void   doAppendRedirection(char* filename);
static void   doStdoutRedirection(char* filename);
static void   doStderrRedirection(char* filename);
static void   doStdoutStderrRedirection(char* filename);
static void   doStdinRedirection(char* filename);
static void   doPipe(char** p1Args, char** line, int* lineIndex);
static void   doLs(char** args);
static void   doRm(char** args);
static void   run(char** args);

/*
 * A global variable representing the process ID of this shell's child.  When the value of this
 * variable is 0, there are no running children.
 */
static pid_t childPid = 0;

/*
 * Entry point of the application
 */
int main(void) {
    char** line;

    /*registerring a custom signal handler function to handle ctrl+shift+c */
    signal(SIGINT, signalHandler);


    /* Read a line of input from the keyboard */
    line = promptAndRead();

    /* While the line was blank or the user didn't type exit */
    while (line[0] == NULL || (strcmp(line[0], "exit") != 0)) {
        int lineIndex = 0; /* An index into the line array */

        /* Ignore blank lines */
        if (line[lineIndex] != NULL) {
            int   status;
            char* args[MAX_ARGS]; /* A processes arguments */

            /* Dig out the arguments for a single process */
            parseArgs(args, line, &lineIndex);

            if (strcmp(args[0], "ls") == 0) {
                doLs(args);
            } else if (strcmp(args[0], "rm") == 0) {
                doRm(args);
            } else {
                /* Fork off a child process */
                childPid = forkWrapper();

                if (CHILD_PID(childPid)) {
                    /* The child shell continues to process the command line */
                    continueProcessingLine(line, &lineIndex, args);
                } else {
                    long wait = 0;
                    do{
                     wait =( long)waitpid(childPid, &status, WUNTRACED);
                    }while(!WIFEXITED(status) && !WIFSIGNALED(status));

                    printf("Child %ld exited with status %d \n", wait, status);



                }
            }
        }

        /* Read the next line of input from the keyboard */
        line = promptAndRead();
    }

    /* User must have typed "exit", time to gracefully exit. */
    return 0;
}


/*
 * continueProcessingLine
 *
 * This function continues to process a line read in from the user.  This processing can include
 * append redirection, stderr redirection, etc.  Note that this function operates recursively; it
 * breaks off a piece associated with a process until it gets to something "special", decides what
 * to do with that "special" thing, and then calls itself to handle the rest.  The base case of
 * the recursion is when the end of the 'line' array is reached (i.e., when line[*lineIndex] ==
 * NULL).
 *
 * line      - An array of pointers to string corresponding to ALL of the tokens entered on the
 * command line.
 * lineIndex - A pointer to the index of the next token to be processed
 * args      - A NULL terminated array of string corresponding to the arguments for a process
 * (i.e., stuff that was already parsed off of line).
 */
static void continueProcessingLine(char** line, int* lineIndex, char** args) {
    if (line[*lineIndex] == NULL) { /* Base case -- nothing left in line */
        run(args);

    } else if (strcmp(line[*lineIndex], ">>") == 0) {
        (*lineIndex)++;
        doAppendRedirection(line[*lineIndex]);

        (*lineIndex)++;
        continueProcessingLine(line, lineIndex, args);

    } else if (strcmp(line[*lineIndex], "2>") == 0) {
        (*lineIndex)++;
        doStderrRedirection(line[*lineIndex]);

        (*lineIndex)++;
        continueProcessingLine(line, lineIndex, args);

    } else if (strcmp(line[*lineIndex], "&>") == 0) {
        (*lineIndex)++;
        doStdoutStderrRedirection(line[*lineIndex]);

        (*lineIndex)++;
        continueProcessingLine(line, lineIndex, args);

    } else if (strcmp(line[*lineIndex], ">") == 0) {
        (*lineIndex)++;
        doStdoutRedirection(line[*lineIndex]);

        (*lineIndex)++;
        continueProcessingLine(line, lineIndex, args);

    } else if (strcmp(line[*lineIndex], "<") == 0) {
        (*lineIndex)++;
        doStdinRedirection(line[*lineIndex]);

        (*lineIndex)++;
        continueProcessingLine(line, lineIndex, args);

    } else if (strcmp(line[*lineIndex], "|") == 0) {
        (*lineIndex)++;
        doPipe(args, line, lineIndex);
        /* doPipe() calls continueProcessingLine() only in some cases */
    }
}

/*
 * doPipe
 *
 * Implements a pipe between two processes.
 *
 * p1Args    - The arguments for the left-hand-side command.
 * line      - An array of pointers to string corresponding to ALL of the
 *             tokens entered on the command line.
 * lineIndex - A pointer to the index of the next token to be processed.
 *             This index should point to one element beyond the pipe
 *             symbol.
 */
static void doPipe(char** p1Args, char** line, int* lineIndex) {
    int   pipefd[2]; /* Array of integers to hold 2 file descriptors. */
    pid_t pid;       /* PID of a child process */

    pipeWrapper(pipefd);

    /* Fork the current process */
    pid = forkWrapper();

    if (CHILD_PID(pid)) { /* Child -- will execute left-hand-side process */
        close(pipefd[0]);//closes child process input side of pipe
        dup2(pipefd[1], STDOUT_FILENO);
        run(p1Args);

    } else {  /* Parent will keep going */
        char* args[MAX_ARGS];
        close(pipefd[1]); //parent closes ouput side of pipe

        printf("right before doing read in parent pipe \n");

        dup2(pipefd[0],STDIN_FILENO);
        //line = read(pipefd[0], lineIndex, FILE_LENGTH); // reads in args

        //delete this next line
        printf("Finish parent pipe ");




        /* Read the args for the next process in the pipeline */
        parseArgs(args, line, lineIndex);

        /* And keep going... */
        continueProcessingLine(line, lineIndex, args);
    }
}

/*
 * doAppendRedirection
 *
 * Redirects the standard output of this process to append to the
 * file with the specified name.
 *
 * filename - the name of the file to which to append our output
 */
static void doAppendRedirection(char* filename) {
    int file = open(filename, O_CREAT | O_APPEND | O_WRONLY, S_IRWXU);

    if(file < 0){
        perror("Error Opening file for append\n");
        _exit(1);
    }

    dup2(file,STDOUT_FILENO);

    close(file);
    
}

/*
 * doStdoutRedirection
 *
 * Redirects the standard output of this process to overwrite the
 * file with the specified name.
 *
 * filename - the name of the file which to overwrite
 */
static void doStdoutRedirection(char* filename) {
    int file = open(filename, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU);

    if(file < 0){
        perror("Error Opening file for Stdout\n");
        _exit(1);
    }

    dup2(file,STDOUT_FILENO);

    close(file);
}

/*
 * doStderrRedirection
 *
 * Redirects the standard error of this process to overwrite the
 * file with the specified name.
 *
 * filename - the name of the file which to overwrite
 */
static void doStderrRedirection(char* filename){
    int file = open(filename, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU);

    if(file < 0){
        perror("Error Opening file for Stdout\n");
        _exit(1);
    }

    dup2(file,STDERR_FILENO);
    close(file);
}

/*
 * doStdoutStderrRedirection
 *
 * Redirects the standard output AND standard error of this process to
 * overwrite the file with the specified name.
 *
 * filename - the name of the file which to overwrite
 */
static void doStdoutStderrRedirection(char* filename) {

    int file = open(filename, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU);

    if(file < 0){
        perror("Error Opening file for Stdout\n");
        _exit(1);
    }

    dup2(file,STDOUT_FILENO);
    dup2(file,STDERR_FILENO);

    close(file);
}

/*
 * doStdinRedirection
 *
 * Redirects the standard input to this process from the file with the
 * specified name.
 *
 * filename - the name of the file from which to read as standard input.
 */
static void doStdinRedirection(char* filename) {
    int file = open(filename, O_RDONLY, S_IRWXU);

    if(file < 0){
        perror("Error Opening file for Stdin\n");
        _exit(1);
    }

    dup2(file,STDIN_FILENO);

    close(file);
}

/*
 * parseArgs
 *
 * Parse the command line, stopping at a special symbol of the end of the line.
 *
 * args      - The array to populate with arguments from line
 * line      - An array of pointers to string corresponding to ALL of the
 *             tokens entered on the command line.
 * lineIndex - A pointer to the index of the next token to be processed.
 *             This index should point to one element beyond the pipe
 *             symbol.
 */
static void parseArgs(char** args, char** line, int* lineIndex) {
    int i;

    for (i = 0;    line[*lineIndex] != NULL
                && !isSpecial(line[*lineIndex]); ++(*lineIndex), ++i) {
        args[i] = line[*lineIndex];
    }
    args[i] = NULL;
}

/*
 * promptAndRead
 *
 * A simple wrapper that displays a prompt and reads a line of input
 * from the user.
 *
 * Returns a pointer to an array of strings where each element in
 * the array corresponds to a token from the input line.
 */
static char** promptAndRead(void) {
    printf("(%d) $ ", getpid());
    return getArgList();
}

/*
 * forkWrapper
 *
 * A simple wrapper around the 'fork' system call that attempts to
 * invoke fork and on failure, prints an appropriate message and
 * terminates the process.
 */
static pid_t forkWrapper(void) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        _exit(2);
    }

    return pid;
}

/*
 * pipeWrapper
 *
 * A simple wrapper around the 'pipe' system call that attempts to invoke
 * pipe and on failure, prints an appropriate message and terminates the
 * process.
 */
static void pipeWrapper(int pipefds[]) {
    int pipeNo = -1;

    if((pipeNo = pipe(pipefds)) < 0) {
        perror("pipe");
        _exit(4);
    }
}

/*
 * dupWrapper
 *
 * A simple wrapper around the 'dup' system call that attempts to invoke
 * pipe and on failure, prints an appropriate message and terminates the
 * process.
 */
static int dupWrapper(int oldfd) {
    int newfd = -1;

    if ((newfd = dup(oldfd)) < 0) {
        perror("dup");
        _exit(3);
    }

    return newfd;
}

/*
 * isSpecial
 *
 * Returns true if the specified token is "special" (i.e., is an
 * operator like >, >>, |, <); false otherwise.
 */
static bool isSpecial(char* token) {
    return    (strlen(token) == 1 && strchr("<>|", token[0]) != NULL)
           || (strlen(token) == 2 && strchr(">",   token[1]) != NULL);
}

/**
 * doLs
 *
 * Implements a built-in version of the 'ls' command.
 *
 * args - An array of strings corresponding to the command and its arguments.
 *        If args[1] is NULL, the current directory (./) is assumed; otherwise
 *        it specifies the directory to list.
 *
 *        NOTE: currently lists all files and subdirectories along with . and ..
 *        pending fix depending on Dr. K's response
 */
static void doLs(char** args) {

    DIR *directory;
    struct dirent *file;

    if(args[1] == NULL){
        directory = opendir("./");
    }else{
        directory = opendir(args[1]);
    }
    if(directory != NULL){
        while((file = readdir(directory)) != NULL){
            printf(file->d_name);
            printf(" \n");
        }
        closedir(directory);
    } else {
        perror("opendir");
    }
}

/**
 * doRm
 *
 * Implements a built-in version of the 'rm' command.
 *
 * args - An array of strings corresponding to the command and its arguments.
 *        args[0] is "rm", additional arguments are in args[1] ... n.
 *        args[x] = NULL indicates the end of the argument list.
 */
static void doRm(char** args) {
    if(args[1] == NULL){
        printf("ERROR: No File Specified \n");
    } else{
        int i = 1;
        while (args[i] != NULL){
            unlink(args[i]);
            i++;
        }
    }
}

/**
 * run
 *
 * runs the program specified by its exact filepath contained in args[0]
 *
 * args - An array of strings corresponding to the command and it's arguments.
 *        args[0] is unknown, but should be a valid process
 *        args[1] - args[n] are additional arguments
 *        args[x] = NULL indicating the end of the argument list
 */
static void run(char** args){
    if(execv(args[0], args) == -1){
            perror("execv");
            _exit(1);
        }
}

/**
 * signalHandler
 *
 * despite the generic name, only handles SIGINT
 */
static void signalHandler(){
    if(PARENT_PID(childPid)){
        kill(SIGINT, childPid);
    }
    //if not, do nothing
}
