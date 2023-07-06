#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <libgen.h>
#include <linux/limits.h>
#include <string.h>
#include <fcntl.h>
#include "systemcalls.h"

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
 * Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/
    int ret = -1;

    ret = system(cmd);
    if (0 == ret)
        return true;
    else
        return false;
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
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

/*
 * Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    int status;
    pid_t pid = -1;

    fflush(stdout);
    pid = fork ();
    if (-1 == pid) {
        /* massive failure */
        perror("fork");
        va_end(args);
        return false;
    }
    else if (0 == pid) {
        /* with a pid of 0 we are in the child process, let's execute our commands */

        /* save of the full path */
        char path[PATH_MAX];
        strcpy(path, command[0]);

        /* cut the full path into the basename only */
        command[0] = basename(command[0]);

        execv(path, &command[0]);

        exit(1);
    }

    if (-1 == waitpid (pid, &status, 0)) {
        /* there was a problem with the child process */
        va_end(args);
        return false;
    }
    va_end(args);

    if (WIFEXITED (status)) {
        /* WIFEXITED will be true if the child terminated normally */
        if (0 == WEXITSTATUS (status))
            return true;
    }

    /* the child have not terminated normally we would have returned true already */
    return false;
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
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

/*
 * Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/

    int status;
    pid_t pid = -1;

    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) {
        /* massive failure */
        perror("open");
        va_end(args);
        return false;
    }

    pid = fork ();
    if (-1 == pid) {
        /* massive failure */
        perror("fork");
        va_end(args);
        close(fd);
        return false;
    }
    else if (0 == pid) {
        /* with a pid of 0 we are in the child process, let's execute our commands */

        /* duplicate the file descriptor and assign it to the std output descriptor */
        if (dup2(fd, 1) < 0) {
            perror("dup2");
            return false;
        }
        close(fd);

        /* save of the full path */
        char path[PATH_MAX];
        strcpy(path, command[0]);

        /* cut the full path into the basename only */
        command[0] = basename(command[0]);

        execv(path, &command[0]);

        exit(1);
    }

    if (-1 == waitpid (pid, &status, 0)) {
        /* there was a problem with the child process */
        va_end(args);
        return false;
    }

    va_end(args);
    close(fd);

    if (WIFEXITED (status)) {
        /* WIFEXITED will be true if the child terminated normally */
        if (0 == WEXITSTATUS (status))
            return true;
    }

    /* the child have not terminated normally we would have returned true already */
    return false;
}

