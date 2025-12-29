#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#define PORT "9000"
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define RECV_BUF_SIZE 1024

int server_fd = -1;
volatile sig_atomic_t caught_signal = 0;

// Requirement 2.i: Signal handler for Graceful Exit
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        caught_signal = 1;
        if (server_fd != -1) {
            // Unblock accept() by shutting down the socket
            shutdown(server_fd, SHUT_RDWR);
        }
    }
}

// Requirement 5: Daemon mode support (Fork after bind)
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(-1);
    if (pid > 0) exit(0); // Parent exits

    if (setsid() < 0) exit(-1); 
    if (chdir("/") < 0) exit(-1);

    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null != -1) {
        dup2(dev_null, STDIN_FILENO);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);
        close(dev_null);
    }
}

int main(int argc, char *argv[]) {
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Setup signals
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Socket setup
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) return -1;

    server_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (server_fd == -1) {
        freeaddrinfo(servinfo);
        return -1;
    }

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    // Requirement 5: Ensure bind is successful before daemonizing
    if (bind(server_fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        close(server_fd);
        freeaddrinfo(servinfo);
        return -1;
    }

    freeaddrinfo(servinfo);

    // Requirement 5: Check for daemon argument
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemonize();
    }

    if (listen(server_fd, 10) == -1) {
        close(server_fd);
        return -1;
    }

    while (!caught_signal) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            if (caught_signal) break;
            continue;
        }

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));
        syslog(LOG_INFO, "Accepted connection from %s", ip_str);

        FILE *f = fopen(DATA_FILE, "a+");
        if (!f) {
            close(client_fd);
            continue;
        }

        char *buf = malloc(RECV_BUF_SIZE);
        if (!buf) {
            fclose(f);
            close(client_fd);
            continue;
        }

        ssize_t bytes_recv;
        size_t current_packet_size = 0;

        // Requirement 2.e: Receive data until newline
        while ((bytes_recv = recv(client_fd, buf + current_packet_size, RECV_BUF_SIZE - 1, 0)) > 0) {
            current_packet_size += bytes_recv;
            if (buf[current_packet_size - 1] == '\n') {
                fwrite(buf, 1, current_packet_size, f);
                fflush(f);
                break; 
            }
            char *new_buf = realloc(buf, current_packet_size + RECV_BUF_SIZE);
            if (!new_buf) {
                free(buf);
                break;
            }
            buf = new_buf;
        }

        // Requirement 2.f: Return full content
        rewind(f);
        char read_buf[RECV_BUF_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(read_buf, 1, sizeof(read_buf), f)) > 0) {
            send(client_fd, read_buf, bytes_read, 0);
        }

        fclose(f);
        free(buf);
        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", ip_str);
    }

    if (server_fd != -1) close(server_fd);
    unlink(DATA_FILE);
    syslog(LOG_INFO, "Cleanup complete");
    closelog();

    return 0;
}
