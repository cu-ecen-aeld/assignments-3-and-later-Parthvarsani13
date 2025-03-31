/****************************************************************************
 * @file aesdsocket.c
 * @brief TCP server for AESD Assignment
 * @author Parth Varsani
 *
 * - Binds to TCP port 9000
 * - Waits for incoming connections
 * - Spawns a new thread for each connection (allowing simultaneous clients)
 * - Receives data, appends to /dev/aesdchar (DATAFILE_PATH) when enabled
 * - On each newline, sends the entire file content back to the client
 * - Logs "Accepted connection from XXX" and "Closed connection from XXX"
 * - Continues in a loop until SIGINT or SIGTERM
 * - On signal, logs "Caught signal, exiting", stops accepting, joins threads,
 *   removes file (if not using aesdchar), and gracefully exits
 * - Supports a -d option to run as a daemon
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include <signal.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>
#include "../aesd-char-driver/aesd_ioctl.h"


#define SERVER_PORT "9000"

#ifdef USE_AESD_CHAR_DEVICE
    #define DATAFILE_PATH "/dev/aesdchar"
#else
    #define DATAFILE_PATH "/var/tmp/aesdsocketdata"
#endif

#define BUF_MAXLEN 1024

static int g_socketfd = -1;
static pthread_mutex_t g_file_mutex;
static volatile sig_atomic_t g_exit_flag = 0;

#ifndef USE_AESD_CHAR_DEVICE
static pthread_t g_timer_thread;
#endif

struct thread_list_node {
    pthread_t thread_id;
    int client_fd;
    struct sockaddr_in client_addr;
    SLIST_ENTRY(thread_list_node) entries;
};

SLIST_HEAD(thread_list_head, thread_list_node) g_thread_list_head;

/**
 * @brief Signal handler to request shutdown on SIGINT/SIGTERM.
 */
static void handle_exit(int sig) {
    syslog(LOG_INFO, "Caught signal %d, exiting", sig);
    g_exit_flag = 1;

    if (g_socketfd != -1) {
        close(g_socketfd);
        g_socketfd = -1;
    }
}

#ifndef USE_AESD_CHAR_DEVICE
/**
 * @brief Timer thread to append timestamp every 10 seconds
 */
static void* timer_thread_func(void* arg) {
    while (!g_exit_flag) {
        sleep(10);
        if (g_exit_flag) break;

        time_t now = time(NULL);
        struct tm* tinfo = localtime(&now);
        if (!tinfo) {
            syslog(LOG_ERR, "localtime failed: %s", strerror(errno));
            continue;
        }

        char timestr[128];
        strftime(timestr, sizeof(timestr), "%a, %d %b %Y %T %z", tinfo);

        pthread_mutex_lock(&g_file_mutex);
        int fd = open(DATAFILE_PATH, O_WRONLY | O_APPEND);
        if (fd >= 0) {
            dprintf(fd, "timestamp:%s\n", timestr);
            close(fd);
        } else {
            syslog(LOG_ERR, "Failed to write timestamp: %s", strerror(errno));
        }
        pthread_mutex_unlock(&g_file_mutex);
    }
    return NULL;
}
#endif

/**
 * @brief Thread function to handle a client connection.
 */
static void* client_thread_func(void *arg) {
    struct thread_list_node *node = (struct thread_list_node *)arg;
    int client_fd = node->client_fd;
    struct sockaddr_in caddr = node->client_addr;
    char *client_ip = inet_ntoa(caddr.sin_addr);
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    char rx_buffer[BUF_MAXLEN + 1];
    ssize_t rx_bytes;

    while ((rx_bytes = recv(client_fd, rx_buffer, BUF_MAXLEN, 0)) > 0) {
        rx_buffer[rx_bytes] = '\0';

#ifdef USE_AESD_CHAR_DEVICE
        if (strncmp(rx_buffer, "AESDCHAR_IOCSEEKTO:", strlen("AESDCHAR_IOCSEEKTO:")) == 0) {
            unsigned int write_cmd, write_cmd_offset;
            char *params = rx_buffer + strlen("AESDCHAR_IOCSEEKTO:");

            if (sscanf(params, "%u,%u", &write_cmd, &write_cmd_offset) == 2) {
                int fd = open(DATAFILE_PATH, O_RDWR);
                if (fd < 0) {
                    syslog(LOG_ERR, "Failed to open %s for ioctl: %s", DATAFILE_PATH, strerror(errno));
                } else {
                    struct aesd_seekto seekto;
                    seekto.write_cmd = write_cmd;
                    seekto.write_cmd_offset = write_cmd_offset;

                    if (ioctl(fd, AESDCHAR_IOCSEEKTO, &seekto) == -1) {
                        syslog(LOG_ERR, "ioctl AESDCHAR_IOCSEEKTO failed: %s", strerror(errno));
                        close(fd);
                        pthread_mutex_unlock(&g_file_mutex);
                        continue;
                    }

                    // Perform a read from the new file position and send to client
                    char send_buffer[BUF_MAXLEN];
                    ssize_t bytes_read;
                    while ((bytes_read = read(fd, send_buffer, BUF_MAXLEN)) > 0) {
                        send(client_fd, send_buffer, bytes_read, 0);
                    }

                    close(fd);
                }
                pthread_mutex_unlock(&g_file_mutex);
                continue; // skip normal write path
            }
        }
#endif

        // Normal write
        pthread_mutex_lock(&g_file_mutex);
        int fd = open(DATAFILE_PATH, O_WRONLY | O_APPEND);
        if (fd < 0) {
            syslog(LOG_ERR, "Failed to open for write: %s", strerror(errno));
            pthread_mutex_unlock(&g_file_mutex);
            break;
        }

        write(fd, rx_buffer, rx_bytes);
        close(fd);
        pthread_mutex_unlock(&g_file_mutex);

        // Echo back only on newline
        if (memchr(rx_buffer, '\n', rx_bytes)) {
            pthread_mutex_lock(&g_file_mutex);
            fd = open(DATAFILE_PATH, O_RDONLY);
            if (fd < 0) {
                syslog(LOG_ERR, "Failed to open for read: %s", strerror(errno));
                pthread_mutex_unlock(&g_file_mutex);
                break;
            }

            char send_buf[BUF_MAXLEN];
            ssize_t bytes_read;
            while ((bytes_read = read(fd, send_buf, BUF_MAXLEN)) > 0) {
                send(client_fd, send_buf, bytes_read, 0);
            }

            close(fd);
            pthread_mutex_unlock(&g_file_mutex);
        }
    }

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    return NULL;
}

/**
 * @brief Run as a daemon process.
 */
static void daemon_run(void) {
    if (fork() > 0) exit(EXIT_SUCCESS);
    if (setsid() < 0) exit(EXIT_FAILURE);
    signal(SIGHUP, SIG_IGN);
    if (fork() > 0) exit(EXIT_SUCCESS);
    umask(0);
    chdir("/");
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[]) {
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    signal(SIGINT, handle_exit);
    signal(SIGTERM, handle_exit);
    pthread_mutex_init(&g_file_mutex, NULL);
    SLIST_INIT(&g_thread_list_head);

    int run_as_daemon = (argc == 2 && strcmp(argv[1], "-d") == 0);
    if (run_as_daemon) daemon_run();

    struct addrinfo hints = {0}, *servinfo;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if (getaddrinfo(NULL, SERVER_PORT, &hints, &servinfo) != 0) {
        syslog(LOG_ERR, "getaddrinfo failed");
        closelog();
        return -1;
    }

    g_socketfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (g_socketfd < 0) return -1;

    setsockopt(g_socketfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if (bind(g_socketfd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) return -1;
    freeaddrinfo(servinfo);
    if (listen(g_socketfd, SOMAXCONN) < 0) return -1;

#ifndef USE_AESD_CHAR_DEVICE
    pthread_create(&g_timer_thread, NULL, timer_thread_func, NULL);
#endif

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    while (!g_exit_flag) {
        int new_fd = accept(g_socketfd, (struct sockaddr*)&client_addr, &addr_len);
        if (new_fd < 0) continue;
        struct thread_list_node *new_node = malloc(sizeof(*new_node));
        new_node->client_fd = new_fd;
        memcpy(&new_node->client_addr, &client_addr, sizeof(client_addr));
        pthread_create(&new_node->thread_id, NULL, client_thread_func, new_node);
        SLIST_INSERT_HEAD(&g_thread_list_head, new_node, entries);
    }

    close(g_socketfd);
    closelog();
    return 0;
}
