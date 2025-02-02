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
    /*
     * TODO  add your code here
     *  Call the system() function with the command set in the cmd
     *   and return a boolean true if the system() call completed with success
     *   or false() if it returned a failure
    */
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

    /*
     * TODO:
     *   Execute a system command by calling fork, execv(),
     *   and wait instead of system (see LSP page 161).
     *   Use the command[0] as the full path to the command to execute
     *   (first argument to execv), and use the remaining arguments
     *   as second argument to the execv() command.
    */

    // Fork a new process
    pid_t pid = fork();

    if (pid == -1) {
        // Fork failed
        va_end(args);
        perror("Fork failed");
        return false;
    }

    if (pid == 0) {
        // Child process
        execv(command[0], command);
        perror("Execv failed");  // If execv fails
        _exit(1);  // Exit with an error code
    } else {
        // Parent process, wait for the child
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            perror("Error waiting for child process");
            va_end(args);
            return false;
        }

        // Check if the child exited successfully
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            va_end(args);
            return true;
        } else {
            va_end(args);
            return false;  // Child process did not exit successfully
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

    /*
     * TODO
     *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a reference,
     *   redirect standard out to a file specified by outputfile.
     *   The rest of the behavior is the same as do_exec()
    */

    // Open the output file for writing
    int out_fd = open(outputfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        perror("Error opening output file");
        va_end(args);
        return false;  // Error opening file
    }

    // Redirect stdout to the output file
    if (dup2(out_fd, STDOUT_FILENO) == -1) {
        perror("Error redirecting stdout");
        close(out_fd);
        va_end(args);
        return false;  // Error redirecting stdout
    }

    // Close the file descriptor since it's no longer needed
    close(out_fd);

    // Fork and execute the command
    pid_t pid = fork();
    if (pid == -1) {
        perror("Error forking process");
        va_end(args);
        return false;
    }
    else if (pid == 0) {
        // Child process
        execv(command[0], command);
        perror("Error executing command");
        _exit(1);  // If execv fails, exit with error code
    } else {
        // Parent process, wait for child
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("Error waiting for child process");
            va_end(args);
            return false;  // Error waiting for child process
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
