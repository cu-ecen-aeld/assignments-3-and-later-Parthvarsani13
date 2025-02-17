/****************************************************************************
 * @file aesdsocket.c
 * @brief TCP server for AESD Assignment
 *
 * - Binds to TCP port 9000
 * - Waits for incoming connections
 * - Receives data, appends to /var/tmp/aesdsocketdata (DATAFILE_PATH)
 * - On each newline, sends the entire file content back to the client
 * - Logs "Accepted connection from XXX" and "Closed connection from XXX"
 * - Continues in a loop until SIGINT or SIGTERM
 * - On signal, logs "Caught signal, exiting", closes sockets, removes file
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

#define SERVER_PORT     "9000"
#define DATAFILE_PATH   "/var/tmp/aesdsocketdata"
#define BUF_MAXLEN      1024

static int g_socketfd  = -1;  // Server socket
static int g_clientfd  = -1;  // Client socket

/**
 * @brief Signal handler to gracefully clean up on SIGINT/SIGTERM.
 *
 * Logs "Caught signal, exiting", closes open sockets, removes data file, 
 * and then exits the process.
 */
static void handle_exit(int sig)
{
    syslog(LOG_INFO, "Caught signal, exiting");

    if (g_clientfd != -1) 
    {
        close(g_clientfd);
        g_clientfd = -1;
    }
    if (g_socketfd != -1) 
    {
        close(g_socketfd);
        g_socketfd = -1;
    }

    remove(DATAFILE_PATH);
    closelog();
    exit(EXIT_SUCCESS);
}

/**
 * @brief Serve an individual client connection.
 * 
 * - Logs "Accepted connection from XXX"
 * - Receives data until client disconnects
 * - Appends data to DATAFILE_PATH
 * - On detecting a newline, sends entire file back to the client
 * - Logs "Closed connection from XXX" on completion
 */
static void serve_client(int client_sock, struct sockaddr_in *client_addr)
{
    char   rx_buffer[BUF_MAXLEN];
    ssize_t rx_bytes = 0;

    FILE *fp_data = fopen(DATAFILE_PATH, "a+");
    if (fp_data == NULL)
    {
        syslog(LOG_ERR, "Failed to open file %s: %s", DATAFILE_PATH, strerror(errno));
        return;
    }

    // Convert client IP address to string
    char *client_ip = inet_ntoa(client_addr->sin_addr);
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    // Read from client until it closes the connection
    while ((rx_bytes = recv(client_sock, rx_buffer, BUF_MAXLEN, 0)) > 0)
    {
        // Write the incoming data to our file
        size_t written = fwrite(rx_buffer, 1, rx_bytes, fp_data);
        if (written < (size_t)rx_bytes)
        {
            syslog(LOG_ERR, "File write error (only wrote %zu of %zu)", written, (size_t)rx_bytes);
        }

        if (fflush(fp_data) == EOF)
        {
            syslog(LOG_ERR, "File flush error: %s", strerror(errno));
        }

        // If we detect a newline, send entire file content back
        if (memchr(rx_buffer, '\n', rx_bytes))
        {
            if (fseek(fp_data, 0, SEEK_SET) != 0)
            {
                syslog(LOG_ERR, "fseek error: %s", strerror(errno));
                continue; // We'll try to keep going
            }

            // Read file in chunks and send to client
            char readbuf[BUF_MAXLEN];
            size_t bytes_read;
            while ((bytes_read = fread(readbuf, 1, BUF_MAXLEN, fp_data)) > 0)
            {
                ssize_t sent = send(client_sock, readbuf, bytes_read, 0);
                if (sent < 0)
                {
                    syslog(LOG_ERR, "Send error: %s", strerror(errno));
                    break; // stop sending on error
                }
            }

            // Reset to end for further appends
            if (fseek(fp_data, 0, SEEK_END) != 0)
            {
                syslog(LOG_ERR, "fseek error while rewinding to end: %s", strerror(errno));
            }
        }
    }

    // When rx_bytes == 0, the client closed connection
    // or if < 0, an error occurred.
    if (rx_bytes < 0)
    {
        syslog(LOG_ERR, "recv error: %s", strerror(errno));
    }

    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    fclose(fp_data);
}

/**
 * @brief Run as a daemon process using the double-fork method.
 *
 * - Fork and exit parent
 * - setsid()
 * - Fork again and exit the first child
 * - chdir("/") and close std I/O
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

    // 3) Check for daemon mode argument
    int run_as_daemon = 0;
    if ((argc == 2) && (strcmp(argv[1], "-d") == 0))
    {
        run_as_daemon = 1;
    }

    // 4) Prepare to bind on port 9000
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

    // 5) Create socket
    g_socketfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (g_socketfd == -1)
    {
        syslog(LOG_ERR, "Socket creation failed: %s", strerror(errno));
        freeaddrinfo(servinfo);
        closelog();
        return -1;
    }

    // 6) Set socket option SO_REUSEADDR
    int optval = 1;
    if (setsockopt(g_socketfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        syslog(LOG_ERR, "setsockopt failed: %s", strerror(errno));
        close(g_socketfd);
        freeaddrinfo(servinfo);
        closelog();
        return -1;
    }

    // 7) Bind to the port
    if (bind(g_socketfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        syslog(LOG_ERR, "bind failed: %s", strerror(errno));
        close(g_socketfd);
        freeaddrinfo(servinfo);
        closelog();
        return -1;
    }
    freeaddrinfo(servinfo);

    // 8) Daemonize if requested
    if (run_as_daemon)
    {
        daemon_run();
    }

    // 9) Listen for connections
    if (listen(g_socketfd, SOMAXCONN) == -1)
    {
        syslog(LOG_ERR, "listen failed: %s", strerror(errno));
        close(g_socketfd);
        closelog();
        return -1;
    }

    syslog(LOG_INFO, "aesdsocket server listening on port %s", SERVER_PORT);

    // 10) Main server loop: accept connections until a signal arrives
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (1)
    {
        g_clientfd = accept(g_socketfd, (struct sockaddr*)&client_addr, &addr_len);
        if (g_clientfd == -1)
        {
            // If accept is interrupted by a signal, break
            if (errno == EINTR)
            {
                break;
            }
            syslog(LOG_ERR, "accept failed: %s", strerror(errno));
            continue;
        }

        // Handle client interaction
        serve_client(g_clientfd, &client_addr);

        // Close client socket
        close(g_clientfd);
        g_clientfd = -1;
    }

    // If we got here, likely a signal triggered EINTR or something else
    handle_exit(0);  // calls exit() internally
    return 0;        // Not reached
}
