#include "systemcalls.h"
#include <stdlib.h>     // system(), exit(), EXIT_FAILURE
#include <stdbool.h>    // bool type
#include <stdarg.h>     // va_list, va_start, va_end
#include <unistd.h>     // fork(), execv(), dup2(), close(), STDOUT_FILENO
#include <sys/wait.h>   // waitpid(), WIFEXITED, WEXITSTATUS
#include <sys/types.h>  // pid_t
#include <fcntl.h>      // open(), O_WRONLY, O_CREAT, O_TRUNC
#include <stdio.h>      // perror()


/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/

bool do_system(const char *cmd)
{
    int ret = system(cmd);
    if (ret == -1) {
        return false; // system() itself failed
    }
    return WIFEXITED(ret) && (WEXITSTATUS(ret) == 0);
}


/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/


bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);

    char *argv[count+1];
    for (int i = 0; i < count; i++) {
        argv[i] = va_arg(args, char *);
    }
    argv[count] = NULL;
    va_end(args);

    pid_t pid = fork();
    if (pid == -1) {
        return false; // fork failed
    }
    if (pid == 0) {
        // child process
        execv(argv[0], argv);
        // if execv returns, it failed
        perror("execv");
        exit(EXIT_FAILURE);
    } else {
        // parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            return false;
        }
        return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
    }
}



/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/


bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);

    char *argv[count+1];
    for (int i = 0; i < count; i++) {
        argv[i] = va_arg(args, char *);
    }
    argv[count] = NULL;
    va_end(args);

    pid_t pid = fork();
    if (pid == -1) {
        return false;
    }
    if (pid == 0) {
        // child process
        int fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            perror("open");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(fd);

        execv(argv[0], argv);
        perror("execv");
        exit(EXIT_FAILURE);
    } else {
        // parent process
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            return false;
        }
        return WIFEXITED(status) && (WEXITSTATUS(status) == 0);
    }
}
