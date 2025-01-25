#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{

    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    if(argc != 3)
    {
        fprintf(stderr, "Error: Missing arguments. Usage: <file_name> <string>\n");
        // priority = LOG_ERR followed by log message
        syslog(LOG_ERR, "Error: Missing arguments. Usage: <file_name> <string>");
        exit(1);
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    // allocate dynamic memory for the directory path like file path
    size_t len = strlen(writefile);
    
    char *dir_path = (char *)malloc(len + 1);    // str + null character
    if (dir_path == NULL)
    {
        syslog(LOG_ERR, "Error: Memory allocation failed for directory path.");
        exit(1);
    }

    strcpy(dir_path, writefile);

    // assuming dir is created by user only
    FILE *file_ptr = fopen(writefile, "w");
    if(file_ptr == NULL)
    {
        syslog(LOG_ERR, "Error: file open '%s' for writing got failed.", writefile);
        free(dir_path);
        exit(1);
    }

    int write_validator = 0;

    write_validator = fprintf(file_ptr, "%s\n", writestr);
    if(write_validator < 0)
    {
        syslog(LOG_ERR, "Error: Failed to write to file '%s'.", writefile);
        fclose(file_ptr);
        free(dir_path);
        exit(1);
    }

    // final message if everything goes well.
    syslog(LOG_DEBUG, "Writing '%s' to '%s'", writestr, writefile);

    fclose(file_ptr);
    free(dir_path);

    closelog();

    return 0;
}