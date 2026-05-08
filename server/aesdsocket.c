#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define PORT 9000
#define BACKLOG 10
#define BUFFER_SIZE 1024
#define DATA_FILE "/var/tmp/aesdsocketdata"

static volatile sig_atomic_t exit_requested = 0;

static void signal_handler(int signo)
{
    (void)signo;
    exit_requested = 1;
}

static int write_all(int fd, const char *buf, size_t len)
{
    size_t total = 0;

    while (total < len) {
        ssize_t written = write(fd, buf + total, len - total);

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        total += (size_t)written;
    }

    return 0;
}

static int send_all(int fd, const char *buf, size_t len)
{
    size_t total = 0;

    while (total < len) {
        ssize_t sent = send(fd, buf + total, len - total, 0);

        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        total += (size_t)sent;
    }

    return 0;
}

static int append_to_file(const char *buf, size_t len)
{
    int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);

    if (fd < 0) {
        syslog(LOG_ERR, "open failed: %s", strerror(errno));
        return -1;
    }

    if (write_all(fd, buf, len) < 0) {
        syslog(LOG_ERR, "write failed: %s", strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

static int send_file_to_client(int client_fd)
{
    int fd = open(DATA_FILE, O_RDONLY);

    if (fd < 0) {
        syslog(LOG_ERR, "open read failed: %s", strerror(errno));
        return -1;
    }

    char buf[BUFFER_SIZE];

    while (1) {
        ssize_t bytes_read = read(fd, buf, sizeof(buf));

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }

            syslog(LOG_ERR, "read failed: %s", strerror(errno));
            close(fd);
            return -1;
        }

        if (bytes_read == 0) {
            break;
        }

        if (send_all(client_fd, buf, (size_t)bytes_read) < 0) {
            syslog(LOG_ERR, "send failed: %s", strerror(errno));
            close(fd);
            return -1;
        }
    }

    close(fd);
    return 0;
}

static int setup_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        return -1;
    }

    if (sigaction(SIGTERM, &sa, NULL) != 0) {
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    return 0;
}

static int create_server_socket(void)
{
    int server_fd;
    int opt = 1;
    struct sockaddr_in server_addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        syslog(LOG_ERR, "socket failed: %s", strerror(errno));
        return -1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) != 0) {
        syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, BACKLOG) != 0) {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }

    return server_fd;
}

static int daemonize(void)
{
    pid_t pid = fork();

    if (pid < 0) {
        return -1;
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        return -1;
    }

    if (chdir("/") != 0) {
        return -1;
    }

    int dev_null = open("/dev/null", O_RDWR);

    if (dev_null >= 0) {
        dup2(dev_null, STDIN_FILENO);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);

        if (dev_null > STDERR_FILENO) {
            close(dev_null);
        }
    }

    return 0;
}

static void handle_client(int client_fd, const char *client_ip)
{
    char buf[BUFFER_SIZE];

    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    while (!exit_requested) {
        ssize_t bytes_received = recv(client_fd, buf, sizeof(buf), 0);

        if (bytes_received < 0) {
            if (errno == EINTR && exit_requested) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            syslog(LOG_ERR, "recv failed: %s", strerror(errno));
            break;
        }

        if (bytes_received == 0) {
            break;
        }

        size_t start = 0;

        for (ssize_t i = 0; i < bytes_received; i++) {
            if (buf[i] == '\n') {
                size_t len = (size_t)i - start + 1;

                if (append_to_file(buf + start, len) != 0) {
                    close(client_fd);
                    syslog(LOG_INFO, "Closed connection from %s", client_ip);
                    return;
                }

                if (send_file_to_client(client_fd) != 0) {
                    close(client_fd);
                    syslog(LOG_INFO, "Closed connection from %s", client_ip);
                    return;
                }

                start = (size_t)i + 1;
            }
        }

        if (start < (size_t)bytes_received) {
            if (append_to_file(buf + start, (size_t)bytes_received - start) != 0) {
                break;
            }
        }
    }

    close(client_fd);
    syslog(LOG_INFO, "Closed connection from %s", client_ip);
}

int main(int argc, char *argv[])
{
    bool daemon_mode = false;

    if (argc == 2) {
        if (strcmp(argv[1], "-d") == 0) {
            daemon_mode = true;
        } else {
            fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
            return -1;
        }
    } else if (argc > 2) {
        fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
        return -1;
    }

    openlog("aesdsocket", LOG_PID, LOG_USER);

    if (setup_signal_handlers() != 0) {
        syslog(LOG_ERR, "signal setup failed: %s", strerror(errno));
        closelog();
        return -1;
    }

    int server_fd = create_server_socket();

    if (server_fd < 0) {
        closelog();
        return -1;
    }

    if (daemon_mode) {
        if (daemonize() != 0) {
            syslog(LOG_ERR, "daemonize failed: %s", strerror(errno));
            close(server_fd);
            closelog();
            return -1;
        }
    }

    while (!exit_requested) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        char client_ip[INET_ADDRSTRLEN];

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (errno == EINTR && exit_requested) {
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            break;
        }

        if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip)) == NULL) {
            strncpy(client_ip, "unknown", sizeof(client_ip));
            client_ip[sizeof(client_ip) - 1] = '\0';
        }

        handle_client(client_fd, client_ip);
    }

    if (exit_requested) {
        syslog(LOG_INFO, "Caught signal, exiting");
    }

    close(server_fd);
    unlink(DATA_FILE);
    closelog();

    return 0;
}