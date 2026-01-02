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
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <sys/queue.h>

#define PORT "9000"
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define RECV_BUF_SIZE 1024

/* * Polyfill for SLIST_FOREACH_SAFE if not provided by the system's <sys/queue.h>
 */
#ifndef SLIST_FOREACH_SAFE
#define SLIST_FOREACH_SAFE(var, head, field, tvar) \
    for ((var) = SLIST_FIRST((head));              \
        (var) && ((tvar) = SLIST_NEXT((var), field), 1); \
        (var) = (tvar))
#endif

int server_fd = -1;
volatile sig_atomic_t caught_signal = 0;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Structure for thread management */
struct thread_info {
    pthread_t thread_id;
    int client_fd;
    bool thread_complete;
    SLIST_ENTRY(thread_info) entries;
};

/* Head of the linked list */
SLIST_HEAD(thread_list, thread_info) head = SLIST_HEAD_INITIALIZER(head);

/* Signal handler for SIGINT and SIGTERM */
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        caught_signal = 1;
        if (server_fd != -1) {
            shutdown(server_fd, SHUT_RDWR);
        }
    }
}

/* Daemonization logic */
void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);
    if (setsid() < 0) exit(EXIT_FAILURE);
    if (chdir("/") < 0) exit(EXIT_FAILURE);

    int dev_null = open("/dev/null", O_RDWR);
    if (dev_null != -1) {
        dup2(dev_null, STDIN_FILENO);
        dup2(dev_null, STDOUT_FILENO);
        dup2(dev_null, STDERR_FILENO);
        close(dev_null);
    }
}

/* 10-second RFC 2822 Timestamp Thread */
void *timestamp_thread(void *arg) {
    while (!caught_signal) {
        // Sleep for 10 seconds, but check for signal frequently to exit fast
        for (int i = 0; i < 10 && !caught_signal; i++) {
            sleep(1);
        }
        if (caught_signal) break;

        time_t now = time(NULL);
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        char time_str[128];
        // Format: "timestamp:Mon, 02 Jan 2006 15:04:05 -0700"
        strftime(time_str, sizeof(time_str), "%a, %d %b %Y %T %z", &tm_info);

        pthread_mutex_lock(&file_mutex);
        FILE *fp = fopen(DATA_FILE, "a");
        if (fp) {
            fprintf(fp, "timestamp:%s\n", time_str);
            fclose(fp);
        }
        pthread_mutex_unlock(&file_mutex);
    }
    return NULL;
}

/* Connection Handler Thread */
void *connection_handler(void *arg) {
    struct thread_info *tinfo = (struct thread_info *)arg;
    char *buf = malloc(RECV_BUF_SIZE);
    if (!buf) goto out;

    size_t buf_size = RECV_BUF_SIZE;
    size_t used = 0;

    while (1) {
        ssize_t rc = recv(tinfo->client_fd, buf + used, buf_size - used, 0);
        if (rc <= 0) break;
        used += rc;

        if (used >= buf_size) {
            buf_size += RECV_BUF_SIZE;
            char *new_buf = realloc(buf, buf_size);
            if (!new_buf) { free(buf); buf = NULL; goto out; }
            buf = new_buf;
        }

        if (buf[used - 1] == '\n') break;
    }

    if (used > 0) {
        pthread_mutex_lock(&file_mutex);
        int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd != -1) {
            write(fd, buf, used);
            close(fd);
        }

        int rfd = open(DATA_FILE, O_RDONLY);
        if (rfd != -1) {
            char read_buf[RECV_BUF_SIZE];
            ssize_t r;
            while ((r = read(rfd, read_buf, sizeof(read_buf))) > 0) {
                send(tinfo->client_fd, read_buf, r, 0);
            }
            close(rfd);
        }
        pthread_mutex_unlock(&file_mutex);
    }

out:
    if (buf) free(buf);
    close(tinfo->client_fd);
    tinfo->thread_complete = true;
    return NULL;
}

int main(int argc, char *argv[]) {
    openlog("aesdsocket", LOG_PID, LOG_USER);

    struct sigaction sa = { .sa_handler = signal_handler };
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM, .ai_flags = AI_PASSIVE };
    struct addrinfo *servinfo;
    if (getaddrinfo(NULL, PORT, &hints, &servinfo) != 0) return -1;

    server_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    if (bind(server_fd, servinfo->ai_addr, servinfo->ai_addrlen) != 0) {
        freeaddrinfo(servinfo);
        return -1;
    }
    freeaddrinfo(servinfo);

    if (argc > 1 && strcmp(argv[1], "-d") == 0) daemonize();

    if (listen(server_fd, 10) != 0) return -1;

    pthread_t ts_tid;
    pthread_create(&ts_tid, NULL, timestamp_thread, NULL);

    while (!caught_signal) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int cfd = accept(server_fd, (struct sockaddr *)&caddr, &clen);
        if (cfd == -1) break;

        struct thread_info *node = malloc(sizeof(struct thread_info));
        node->client_fd = cfd;
        node->thread_complete = false;

        if (pthread_create(&node->thread_id, NULL, connection_handler, node) == 0) {
            SLIST_INSERT_HEAD(&head, node, entries);
        } else {
            close(cfd);
            free(node);
        }

        /* Safe Cleanup of finished threads */
        struct thread_info *it, *tmp;
        SLIST_FOREACH_SAFE(it, &head, entries, tmp) {
            if (it->thread_complete) {
                pthread_join(it->thread_id, NULL);
                SLIST_REMOVE(&head, it, thread_info, entries);
                free(it);
            }
        }
    }

    /* Shutdown and Wait for all threads */
    pthread_join(ts_tid, NULL);
    struct thread_info *it, *tmp;
    SLIST_FOREACH_SAFE(it, &head, entries, tmp) {
        pthread_join(it->thread_id, NULL);
        SLIST_REMOVE(&head, it, thread_info, entries);
        free(it);
    }

    unlink(DATA_FILE);
    pthread_mutex_destroy(&file_mutex);
    closelog();
    return 0;
}
