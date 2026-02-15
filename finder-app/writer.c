#include <stdio.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    openlog("writer", LOG_PID | LOG_CONS, LOG_USER);

    if (argc != 3)
    {
        const char *msg = "Missing arguments. Usage: <file_name> <string>";
        fprintf(stderr, "Error: %s\n", msg);
        syslog(LOG_ERR, "%s", msg);
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr  = argv[2];
    size_t count = (size_t)strlen(writestr);
    ssize_t nr;
    int fd;

    fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC , 0644);
    if (fd == -1)
    {
        fprintf(stderr, "Error: open('%s') failed: %s\n", writefile, strerror(errno));
        syslog(LOG_ERR, "open('%s') failed: %s", writefile, strerror(errno));
        closelog();
        return 1;
    }

    nr = write(fd, writestr, count);
    if (nr == -1)
    {
        fprintf(stderr, "Error: write('%s') failed: %s\n", writefile, strerror(errno));
        syslog(LOG_ERR, "write('%s') failed: %s", writefile, strerror(errno));
        closelog();
        close(fd);
        return 1;
    }
    else if ((size_t)nr != count)
    {
        fprintf(stderr, "Error: partial write to '%s' (%zd of %zu bytes)\n", writefile, nr, count);
        syslog(LOG_ERR, "partial write to '%s' (%zd of %zu bytes)", writefile, nr, count);
        closelog();
        close(fd);
        return 1;
    }

    syslog(LOG_DEBUG, "Writing '%s' to '%s'", writestr, writefile);

    close(fd);
    closelog();

    return 0;
}
