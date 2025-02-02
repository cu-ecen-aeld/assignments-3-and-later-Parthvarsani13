#include "systemcalls.h"
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    int cmd_ret = system(cmd);

    if (cmd_ret == -1) {
        perror("system failed");
        return false;
    }

    if (cmd_ret == 0)
        return true;
    else
        return false;
}

/**
* @param count - The number of variables passed to the function. The variables are command to execute,
*   followed by arguments to pass to the command.
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
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    pid_t pid = fork();

    if (pid == -1) {
        va_end(args);
        perror("Fork failed");
        return false;
    }

    if (pid == 0) {
        execv(command[0], command);
        perror("Execv failed");
        _exit(1);
    } else {
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            perror("Error waiting for child process");
            va_end(args);
            return false;
        }

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            va_end(args);
            return true;
        } else {
            va_end(args);
            return false;
        }
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
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++) {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    // Fork a new process
    pid_t pid = fork();
    if (pid == -1) {
        perror("Error forking process");
        va_end(args);
        return false;
    }
    else if (pid == 0) {
        // Child process
        // Open the output file for writing (we only open it in the child process)
        int out_fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out_fd < 0) {
            perror("Error opening output file");
            va_end(args);
            _exit(1);  // Exit with an error code if file opening fails
        }

        // Redirect stdout to the output file
        if (dup2(out_fd, STDOUT_FILENO) == -1) {
            perror("Error redirecting stdout");
            close(out_fd);
            va_end(args);
            _exit(1);  // Exit with an error code if redirection fails
        }

        // Close the output file descriptor as we no longer need it
        close(out_fd);

        // Execute the command
        execv(command[0], command);
        perror("Error executing command");
        _exit(1);  // If execv fails, exit with an error code
    } else {
        // Parent process, wait for the child
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("Error waiting for child process");
            va_end(args);
            return false;
        }

        // Check the child's exit status
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            va_end(args);
            return true;  // Success
        } else {
            va_end(args);
            return false;  // Failure
        }
    }
}
