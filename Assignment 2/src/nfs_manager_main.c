#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "../include/nfs_manager.h"
#include "../include/util.h"
#include "../include/sync_buffer.h"
#include "../include/socket_util.h"

#define WORKER_LIMIT_DEFAULT 5
#define BUF_SIZE 4096

extern char *optarg;

int main(int argc, char *argv[]) {

    char *log_name = NULL;
    char *config_name = NULL;
    int worker_limit = -1;
    int console_port = -1;
    int buffer_size = -1;

    // Parse arguments
    int opt;
    while ((opt = getopt(argc, argv, "l:c:n:p:b:")) != -1) {
        switch(opt) {
            case 'l':
                log_name = optarg;
                break;
            case 'c':
                config_name = optarg;
                break;
            case 'n':
                worker_limit = atoi(optarg);
                break;
            case 'p':
                console_port = atoi(optarg);
                break;
            case 'b':
                buffer_size = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -l <manager_logfile> -c <config_file> -n <worker_limit> -p <port_number> -b <bufferSize>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (log_name == NULL || config_name == NULL) {
        fprintf(stderr, "Usage: %s -l <manager_logfile> -c <config_file> -n <worker_limit> -p <port_number> -b <bufferSize>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (worker_limit < 0)
        worker_limit = WORKER_LIMIT_DEFAULT;

    if (console_port < 0) {
        fprintf(stderr, "Please give a valid port number.\n");
        exit(EXIT_FAILURE);
    }

    if (buffer_size < 0) {
        fprintf(stderr, "Please give a valid buffer size.\n");
        exit(EXIT_FAILURE);
    }

    int err = 0;

    // Initialize sync buffer
    if (sync_buffer_init(buffer_size) < 0) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Create worker thread pool
    pthread_t *worker_pool = nfs_create_worker_thread_pool(worker_limit, &err);
    if (worker_pool == NULL) {
        fprintf(stderr, "Memory allocation failed: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }

    if (err) {
        fprintf(stderr, "Failed to create thread: %s\n", strerror(err));
        exit(EXIT_FAILURE);
    }

    int sock;
    enum socket_error sock_err_code;

    // Open socket for console
    sock_err_code = socket_server_create(console_port, &sock, &err);
    if (sock_err_code != SOCKET_SUCCESS) {
        print_sock_err_message(sock_err_code, err, NULL);
        exit(EXIT_FAILURE);
    }

    // Read entries from config file
    nfs_read_config_file(config_name, log_name);

    // Start receiving commands from console until shutdown
    int con_sock;
    nfs_connect_to_console(sock, &con_sock);

    // Join all worker threads to main thread
    nfs_join_worker_thread_pool(worker_pool, worker_limit);
    free(worker_pool);

    sync_buffer_destroy();

    nfs_send_shutdown_complete_message(con_sock);
    close(sock); close(con_sock);
}

