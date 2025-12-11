#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
    // Open syslog with LOG_USER facility
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    // Check arguments
    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments. Usage: %s <writefile> <writestr>", argv[0]);
        fprintf(stderr, "Error: Invalid number of arguments.\nUsage: %s <writefile> <writestr>\n", argv[0]);
        closelog();
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr  = argv[2];

    // Try to open file for writing
    FILE *fp = fopen(writefile, "w");
    if (fp == NULL) {
        syslog(LOG_ERR, "Failed to open file %s for writing", writefile);
        perror("Error opening file");
        closelog();
        return 1;
    }

    // Write string to file
    if (fputs(writestr, fp) == EOF) {
        syslog(LOG_ERR, "Failed to write string to file %s", writefile);
        perror("Error writing to file");
        fclose(fp);
        closelog();
        return 1;
    }

    // Log success with LOG_DEBUG
    syslog(LOG_DEBUG, "Writing \"%s\" to %s", writestr, writefile);

    fclose(fp);
    closelog();
    return 0;
}
