/****************************************************************************
 * @file aesdsocket.c
 * @brief TCP server for AESD Assignment
 * @author Parth Varsani
 *
 * - Binds to TCP port 9000
 * - Waits for incoming connections
 * - Spawns a new thread for each connection (allowing simultaneous clients)
 * - Receives data, appends to /var/tmp/aesdsocketdata (DATAFILE_PATH) using a mutex
 * - On each newline, sends the entire file content back to the client
 * - Logs "Accepted connection from XXX" and "Closed connection from XXX"
 * - Appends a timestamp line "timestamp: <RFC2822 time>" every 10 seconds
 * - Continues in a loop until SIGINT or SIGTERM
 * - On signal, logs "Caught signal, exiting", stops accepting, joins threads,
 *   removes file, and gracefully exits
 * - Supports a -d option to run as a daemon
 * 
 * @ref Implemented these code, taking reference from Jainil Patel, (Learned this method)
 *   1) Using O_TRUNC at startup to ensure a fresh file, similar to the approach in the 
 *      reference code snippet.
 *   2) Calling shutdown(client_fd, SHUT_RDWR) before close(), as shown in the reference 
 *      code snippet, for a more graceful socket shutdown.
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

#define SERVER_PORT     "9000"

#ifdef USE_AESD_CHAR_DEVICE
    #define DATAFILE_PATH "/dev/aesdchar"
#else
    #define DATAFILE_PATH "/var/tmp/aesdsocketdata"
#endif

#define BUF_MAXLEN      1024

// Global listening socket
static int g_socketfd  = -1;

// Mutex for synchronizing file access
static pthread_mutex_t g_file_mutex;

// Global flag set by signal handler to indicate shutdown
static volatile sig_atomic_t g_exit_flag = 0;

// Thread for periodic timestamp
#ifndef USE_AESD_CHAR_DEVICE
static pthread_t g_timer_thread;
#endif

// Structure for storing thread info in a singly linked list
struct thread_list_node {
    pthread_t thread_id;
    int client_fd;
    struct sockaddr_in client_addr;  
    SLIST_ENTRY(thread_list_node) entries;
};

// Singly linked list head for client threads
SLIST_HEAD(thread_list_head, thread_list_node) g_thread_list_head;

/**
 * @brief Signal handler to gracefully request shutdown on SIGINT/SIGTERM.
 */
static void handle_exit(int sig)
{
    syslog(LOG_INFO, "Caught signal %d, exiting", sig);
    g_exit_flag = 1;

    // Close listening socket so accept() will break
    if (g_socketfd != -1) {
        close(g_socketfd);
        g_socketfd = -1;
    }
    // We do NOT call exit() here; we let main join threads, remove file, etc.
}

#ifndef USE_AESD_CHAR_DEVICE
/**
 * @brief Timer thread function
 *        Every 10 seconds, appends a timestamp line to DATAFILE_PATH
 */
static void* timer_thread_func(void* arg)
{
    while (!g_exit_flag)
    {
        // Sleep 10 seconds
        sleep(10);

        // If exit flag was set during sleep, break out
        if (g_exit_flag) break;

        // Build the timestamp string
        time_t now = time(NULL);
        struct tm* tinfo = localtime(&now);
        if (!tinfo) {
            syslog(LOG_ERR, "localtime failed: %s", strerror(errno));
            continue; // Skip this round
        }

        char timestr[128];
        // Example RFC 2822-like format: "Wed, 07 Dec 2025 13:05:59 -0500"
        strftime(timestr, sizeof(timestr), "%a, %d %b %Y %T %z", tinfo);

        // Lock the file mutex
        pthread_mutex_lock(&g_file_mutex);

        // Append to file
        FILE *fp = fopen(DATAFILE_PATH, "a");
        if (fp) {
            fprintf(fp, "timestamp:%s\n", timestr);
            fflush(fp);
            fclose(fp);
        } else {
            syslog(LOG_ERR, "Failed to open file for timestamp: %s", strerror(errno));
        }

        // Unlock
        pthread_mutex_unlock(&g_file_mutex);
    }
    return NULL;
}
#endif

/**
 * @brief Thread function to handle one client connection.
 *
 * - Logs "Accepted connection from XXX"
 * - Receives data, writes to /var/tmp/aesdsocketdata under a mutex
 * - On newline, sends entire file content back
 * - Logs "Closed connection from XXX" when done
 */
static void* client_thread_func(void *arg)
{
    struct thread_list_node *node = (struct thread_list_node *)arg;
    int client_fd = node->client_fd;
    struct sockaddr_in caddr = node->client_addr;

    // Convert client IP address to string
    char *client_ip = inet_ntoa(caddr.sin_addr);
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    // Open the data file (or device)
    FILE *fp_data = fopen(DATAFILE_PATH, "a+");
    if (!fp_data) {
        syslog(LOG_ERR, "Failed to open file %s: %s", DATAFILE_PATH, strerror(errno));
        close(client_fd);
        return NULL;
    }

    char rx_buffer[BUF_MAXLEN];
    ssize_t rx_bytes;

    // Receive data in a loop
    while ((rx_bytes = recv(client_fd, rx_buffer, BUF_MAXLEN, 0)) > 0)
    {
        pthread_mutex_lock(&g_file_mutex);

        // Write the incoming data
        size_t written = fwrite(rx_buffer, 1, rx_bytes, fp_data);
        if (written < (size_t)rx_bytes) {
            syslog(LOG_ERR, "File write error (only wrote %zu of %zu)", written, (size_t)rx_bytes);
        }
        fflush(fp_data);

        pthread_mutex_unlock(&g_file_mutex);

        // If we detect a newline, read **only the last written line**
        if (memchr(rx_buffer, '\n', rx_bytes))
        {
            pthread_mutex_lock(&g_file_mutex);

#ifdef USE_AESD_CHAR_DEVICE
            // For /dev/aesdchar, read only the last line written
            FILE *fp_read = fopen(DATAFILE_PATH, "r");
#else
            // For regular file, seek to the last line
            if (fseek(fp_data, 0, SEEK_SET) != 0) {
                syslog(LOG_ERR, "fseek error: %s", strerror(errno));
            }
            FILE *fp_read = fp_data;
#endif

            if (fp_read) {
                char last_line[BUF_MAXLEN] = {0};
                while (fgets(last_line, sizeof(last_line), fp_read)) {
                    // Keep reading until the last line
                }

                // Send only the last line to the client
                ssize_t sent = send(client_fd, last_line, strlen(last_line), 0);
                if (sent < 0) {
                    syslog(LOG_ERR, "Send error: %s", strerror(errno));
                }

                // Close file if it was reopened
#ifdef USE_AESD_CHAR_DEVICE
                fclose(fp_read);
#endif
            }

            pthread_mutex_unlock(&g_file_mutex);
        }
    }

    if (rx_bytes < 0) {
        syslog(LOG_ERR, "recv error from %s: %s", client_ip, strerror(errno));
    }

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    fclose(fp_data);

    return NULL;
}



/**
 * @brief Run as a daemon process using the double-fork method.
 */
static void daemon_run(void)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        syslog(LOG_ERR, "First fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0)
    {
        // Parent exits
        exit(EXIT_SUCCESS);
    }

    // Child becomes session leader
    if (setsid() < 0)
    {
        syslog(LOG_ERR, "setsid failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0)
    {
        syslog(LOG_ERR, "Second fork failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0)
    {
        // Original child exits
        exit(EXIT_SUCCESS);
    }

    // File mode mask
    umask(0);
    chdir("/");

    // Close standard I/O
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[])
{
    // 1) Open syslog
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);

    // 2) Register signal handlers for graceful exit
    signal(SIGINT, handle_exit);
    signal(SIGTERM, handle_exit);

    // 3) Initialize the mutex and singly linked list
    pthread_mutex_init(&g_file_mutex, NULL);
    SLIST_INIT(&g_thread_list_head);

    // 4) Check for daemon mode argument
    int run_as_daemon = 0;
    if ((argc == 2) && (strcmp(argv[1], "-d") == 0))
    {
        run_as_daemon = 1;
    }

#ifndef USE_AESD_CHAR_DEVICE
    // 5) Truncate (or create) the data file at startup
    {
        int fd = open(DATAFILE_PATH, O_CREAT | O_RDWR | O_TRUNC, 0666);
        if (fd < 0) {
            syslog(LOG_ERR, "Failed to open/create %s: %s", DATAFILE_PATH, strerror(errno));
            closelog();
            return -1;
        }
        close(fd);
    }
#endif

    // 6) Daemonize if requested
    if (run_as_daemon)
    {
        daemon_run();
    }

    // 7) Prepare to bind on port 9000
    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;      // IPv4
    hints.ai_socktype = SOCK_STREAM;  // TCP
    hints.ai_flags    = AI_PASSIVE;   // For bind

    int rv = getaddrinfo(NULL, SERVER_PORT, &hints, &servinfo);
    if (rv != 0)
    {
        syslog(LOG_ERR, "getaddrinfo failed: %s", gai_strerror(rv));
        closelog();
        return -1;
    }

    // 8) Create socket
    g_socketfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (g_socketfd == -1)
    {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        freeaddrinfo(servinfo);
        closelog();
        return -1;
    }

    // 9) Set socket option SO_REUSEADDR
    int optval = 1;
    if (setsockopt(g_socketfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
        close(g_socketfd);
        freeaddrinfo(servinfo);
        closelog();
        return -1;
    }

    // 10) Bind to the port
    if (bind(g_socketfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        close(g_socketfd);
        freeaddrinfo(servinfo);
        closelog();
        return -1;
    }
    freeaddrinfo(servinfo);

    // 11) Listen for connections
    if (listen(g_socketfd, SOMAXCONN) == -1)
    {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(g_socketfd);
        closelog();
        return -1;
    }

    syslog(LOG_INFO, "aesdsocket server listening on port %s", SERVER_PORT);

#ifndef USE_AESD_CHAR_DEVICE
    // 12) Create the timer thread (for 10-second timestamps)
    if (pthread_create(&g_timer_thread, NULL, timer_thread_func, NULL) != 0)
    {
        syslog(LOG_ERR, "Failed to create timer thread: %s", strerror(errno));
        // We could continue, but no timestamps would be written
    }
#endif

    // 13) Main server loop: accept connections until g_exit_flag is set
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (!g_exit_flag)
    {
        int new_fd = accept(g_socketfd, (struct sockaddr*)&client_addr, &addr_len);
        if (new_fd == -1)
        {
            // If accept is interrupted by a signal (EINTR), check if we are shutting down
            if (errno == EINTR && g_exit_flag)
            {
                break; // exit loop
            }
            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }

        // Allocate a new list node for this client thread
        struct thread_list_node *new_node = malloc(sizeof(*new_node));
        if (!new_node)
        {
            syslog(LOG_ERR, "Failed to allocate memory for thread node");
            close(new_fd);
            continue;
        }
        memset(new_node, 0, sizeof(*new_node));
        new_node->client_fd = new_fd;
        memcpy(&new_node->client_addr, &client_addr, sizeof(client_addr));

        // Create the client thread
        if (pthread_create(&new_node->thread_id, NULL, client_thread_func, new_node) != 0)
        {
            syslog(LOG_ERR, "pthread_create failed: %s", strerror(errno));
            close(new_fd);
            free(new_node);
            continue;
        }

        // Insert the new node into the singly linked list
        SLIST_INSERT_HEAD(&g_thread_list_head, new_node, entries);
    }

    // 14) Stop accepting new connections, join existing client threads
    struct thread_list_node *node;
    while (!SLIST_EMPTY(&g_thread_list_head))
    {
        node = SLIST_FIRST(&g_thread_list_head);
        pthread_join(node->thread_id, NULL); // Wait for the client thread to finish
        SLIST_REMOVE_HEAD(&g_thread_list_head, entries);
        free(node);
    }

    // 15) Join the timer thread
#ifndef USE_AESD_CHAR_DEVICE
    pthread_join(g_timer_thread, NULL);
#endif


    // 16) Final cleanup
    if (g_socketfd != -1)
    {
        close(g_socketfd);
    }

    // Remove the data file
    remove(DATAFILE_PATH);

    // Destroy the mutex
    pthread_mutex_destroy(&g_file_mutex);

    // Close syslog
    closelog();

    return 0;
}
