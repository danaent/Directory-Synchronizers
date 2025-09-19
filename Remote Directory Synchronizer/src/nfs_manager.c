#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include "../include/util.h"
#include "../include/socket_util.h"
#include "../include/sync_buffer.h"
#include "../include/nfs_manager.h"
#include "../include/logfile.h"

#define BUF_SIZE 4096

LogFile log_file;


// Function for each worker thread
void *worker_thread(void *ptr) {

    int err;
    struct sync_pair pair;
    char buffer[BUF_SIZE];
    char datetime[DATETIME_SZ];
    
    while (1) {

        // Get pair from buffer
        sync_buffer_obtain(&pair);

        // If empty pair was returned
        if (pair.file[0] == '\0') break;

        // Connect to sockets
        int src_sock; int tar_sock;
        enum socket_error sock_err_code;

        sock_err_code = socket_client_connect(pair.src_dir.host, pair.src_dir.port, &src_sock, &err);
        if (sock_err_code != SOCKET_SUCCESS) {
            printf("Couldn't connect to %s:%d : %s\n", pair.src_dir.host, pair.src_dir.port, strerror(err));
            continue;
        }

        // Send command PULL to source nfs_client
        snprintf(buffer, BUF_SIZE, "PULL %s/%s ", pair.src_dir.dir, pair.file);
        write_bytes(src_sock, buffer, strlen(buffer));

        // Read file size
        uint32_t network_file_size;
        int32_t file_size;

        read_bytes(src_sock, (char *) &network_file_size, sizeof(network_file_size));
        read_bytes(src_sock, buffer, 1); // white character

        file_size = (int32_t) ntohl(network_file_size);

        // If file size is -1, an error has occured
        if (file_size < 0) {
            // Small buffer for error
            char err_buff[512];
            read_line(src_sock, err_buff, 512);
            err_buff[strlen(err_buff)-1] = '\0';

            // Log error
            get_date_time(datetime, DATETIME_SZ);

            snprintf(buffer, BUF_SIZE, "[%s] [%s/%s@%s:%d] [%s/%s@%s:%d] [%lu] [PULL] [ERROR] [%s]\n",
            datetime, pair.src_dir.dir, pair.file, pair.src_dir.host, pair.src_dir.port,
            pair.tar_dir.dir, pair.file, pair.tar_dir.host, pair.tar_dir.port, (unsigned long) pthread_self(), err_buff);

            logfile_write(log_file, buffer, strlen(buffer));

            close(src_sock);
            continue;
        }
        
        // Log successful PULL
        get_date_time(datetime, DATETIME_SZ);

        snprintf(buffer, BUF_SIZE, "[%s] [%s/%s@%s:%d] [%s/%s@%s:%d] [%lu] [PULL] [SUCCESS] [%ld bytes pulled]\n",
        datetime, pair.src_dir.dir, pair.file, pair.src_dir.host, pair.src_dir.port,
        pair.tar_dir.dir, pair.file, pair.tar_dir.host, pair.tar_dir.port, (unsigned long) pthread_self(), (long int) file_size);

        logfile_write(log_file, buffer, strlen(buffer));

        // Open target socket
        sock_err_code = socket_client_connect(pair.tar_dir.host, pair.tar_dir.port, &tar_sock, &err);
        if (sock_err_code != SOCKET_SUCCESS) {
            printf("Couldn't connect to %s:%d : %s\n", pair.src_dir.host, pair.src_dir.port, strerror(err));
            close(src_sock);
            continue;
        }

        ssize_t bytes_left = file_size; // Bytes left to read from source file
        ssize_t bytes_to_read = bytes_left < BUF_SIZE ? bytes_left : BUF_SIZE; // Bytes that will be read in this iteration

        // Small buffer for PUSH command
        int comm_buff_size = DIR_SIZE + FILE_SIZE + 10;
        char comm_buffer[comm_buff_size];

        // First PUSH, with chunk size -1
        // This will either create the file, if it's non-existent, or truncate it, if it exists
        snprintf(comm_buffer, comm_buff_size, "PUSH %s/%s -1 ", pair.tar_dir.dir, pair.file);
        write_bytes(tar_sock, comm_buffer, strlen(comm_buffer));

        // Log PUSH
        get_date_time(datetime, DATETIME_SZ);

        snprintf(buffer, BUF_SIZE, "[%s] [%s/%s@%s:%d] [%s/%s@%s:%d] [%lu] [PUSH] [SUCCESS] [0 bytes pushed]\n",
        datetime, pair.src_dir.dir, pair.file, pair.src_dir.host, pair.src_dir.port,
        pair.tar_dir.dir, pair.file, pair.tar_dir.host, pair.tar_dir.port, (unsigned long) pthread_self());

        logfile_write(log_file, buffer, strlen(buffer));

        close(tar_sock);

        // Pull and push entire file
        while (bytes_left != 0) {

            // Read from source socket
            read_bytes(src_sock, buffer, bytes_to_read);

            // Connect to target socket
            sock_err_code = socket_client_connect(pair.tar_dir.host, pair.tar_dir.port, &tar_sock, &err);
            if (sock_err_code != SOCKET_SUCCESS) {
                printf("Couldn't connect to %s:%d : %s\n", pair.src_dir.host, pair.src_dir.port, strerror(err));
                close(src_sock);
                continue;
            }

            // Send PUSH command
            snprintf(comm_buffer, comm_buff_size, "PUSH %s/%s %ld ", pair.tar_dir.dir, pair.file, bytes_to_read);
            write_bytes(tar_sock, comm_buffer, strlen(comm_buffer));

            // Write to target socket
            write_bytes(tar_sock, buffer, bytes_to_read);

            // Log PUSH
            get_date_time(datetime, DATETIME_SZ);

            snprintf(buffer, BUF_SIZE, "[%s] [%s/%s@%s:%d] [%s/%s@%s:%d] [%lu] [PUSH] [SUCCESS] [%ld bytes pushed]\n",
            datetime, pair.src_dir.dir, pair.file, pair.src_dir.host, pair.src_dir.port,
            pair.tar_dir.dir, pair.file, pair.tar_dir.host, pair.tar_dir.port, (unsigned long) pthread_self(), (long int) bytes_to_read);

            logfile_write(log_file, buffer, strlen(buffer));

            close(tar_sock);

            bytes_left -= bytes_to_read;
            bytes_to_read = bytes_left < BUF_SIZE ? bytes_left : BUF_SIZE;
        }

        close(src_sock);

        // Connect to target socket
        sock_err_code = socket_client_connect(pair.tar_dir.host, pair.tar_dir.port, &tar_sock, &err);
        if (sock_err_code != SOCKET_SUCCESS) {
            printf("Couldn't connect to %s:%d : %s\n", pair.src_dir.host, pair.src_dir.port, strerror(err));
            continue;
        }

        // Final PUSH
        snprintf(comm_buffer, comm_buff_size, "PUSH %s/%s 0 ", pair.tar_dir.dir, pair.file);
        write_bytes(tar_sock, comm_buffer, strlen(comm_buffer));

        close(tar_sock);

        // Log PUSH
        get_date_time(datetime, DATETIME_SZ);

        snprintf(buffer, BUF_SIZE, "[%s] [%s/%s@%s:%d] [%s/%s@%s:%d] [%lu] [PUSH] [SUCCESS] [0 bytes pushed]\n",
        datetime, pair.src_dir.dir, pair.file, pair.src_dir.host, pair.src_dir.port,
        pair.tar_dir.dir, pair.file, pair.tar_dir.host, pair.tar_dir.port, (unsigned long) pthread_self());

        logfile_write(log_file, buffer, strlen(buffer));
    }

    pthread_exit(NULL);
}

pthread_t *nfs_create_worker_thread_pool(int worker_limit, int *err) {
    pthread_t *worker_pool = malloc(worker_limit * sizeof(pthread_t));
    if (worker_pool == NULL) {
        *err = errno;
        return NULL;
    }

    for (int i = 0; i < worker_limit; i++) {
        *err = pthread_create(&worker_pool[i], NULL, worker_thread, NULL);
        if (*err) break;
    }

    return worker_pool;
}

void nfs_read_config_file(char *config_name, char *log_name) {

    // Open config file for reading
    FILE *config_file = fopen(config_name, "r");  

    if (config_file == NULL) {
        fprintf(stderr, "Failed to open config file %s: %s\n", config_name, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Intiialize log file
    if (logfile_init(&log_file, log_name) < 0) {
        fprintf(stderr, "Failed to open log file %s: %s\n", log_name, strerror(errno));
        exit(EXIT_FAILURE);
    }

    char buffer[BUF_SIZE];
    int err;

    // Read line by line
    while (fgets(buffer, BUF_SIZE, config_file)) {
        struct sync_pair pair;

        // Get source and target directory, host and port
        if (sscanf(buffer, " (%255[^@]@%255[^:]:%d, %255[^@]@%255[^:]:%d)", 
            pair.src_dir.dir, pair.src_dir.host, &(pair.src_dir.port),
            pair.tar_dir.dir, pair.tar_dir.host, &(pair.tar_dir.port)) == 6) {

                // Connect to socket
                int src_sock;
                enum socket_error sock_err_code;

                sock_err_code = socket_client_connect(pair.src_dir.host, pair.src_dir.port, &src_sock, &err);
                if (sock_err_code != SOCKET_SUCCESS) {
                    printf("Couldn't connect to %s:%d : %s\n", pair.src_dir.host, pair.src_dir.port, strerror(err));
                    continue;
                }

                // Send LIST command
                snprintf(buffer, BUF_SIZE, "LIST %s ", pair.src_dir.dir);
                write_bytes(src_sock, buffer, strlen(buffer));

                // Read line by line
                read_line(src_sock, pair.file, FILE_SIZE);
                pair.file[strlen(pair.file)-1] = '\0';

                while (strcmp(pair.file, ".")) {

                    // If file is already in buffer
                    if (sync_buffer_file_exists(pair)) {
                        // Write message to buffer
                        char datetime[DATETIME_SZ];
                        get_date_time(datetime, DATETIME_SZ);

                        snprintf(buffer, BUF_SIZE, "[%s] Already in queue: %s/%s@%s:%d\n",
                        datetime, pair.src_dir.dir, pair.file, pair.src_dir.host, pair.src_dir.port);

                    // If file is not in buffer
                    } else {
                        // Add to sync buffer
                        sync_buffer_place(pair);

                        // Write message to buffer
                        char datetime[DATETIME_SZ];
                        get_date_time(datetime, DATETIME_SZ);

                        snprintf(buffer, BUF_SIZE, "[%s] Added file: %s/%s@%s:%d -> %s/%s@%s:%d\n",
                        datetime, pair.src_dir.dir, pair.file, pair.src_dir.host, pair.src_dir.port,
                        pair.tar_dir.dir, pair.file, pair.tar_dir.host, pair.tar_dir.port);

                        // Write to log file
                        logfile_write(log_file, buffer, strlen(buffer));
                    }

                    printf("%s", buffer);

                    // Read next line
                    read_line(src_sock, pair.file, FILE_SIZE);
                    pair.file[strlen(pair.file)-1] = '\0';
                }

                close(src_sock);

        } else {
            fprintf(stderr, "Invalid format in config file %s\n", config_name);
            fprintf(stderr, "%s %s %d %s %s %d\n", pair.src_dir.dir, pair.src_dir.host, pair.src_dir.port,
            pair.tar_dir.dir, pair.tar_dir.host, pair.tar_dir.port);
            fclose(config_file);
            exit(EXIT_FAILURE);
        }
    }

    fclose(config_file);
}

void nfs_connect_to_console(int sock, int *ret_con_sock) {

    int err;

    // Accept connection from console
    int con_sock = accept(sock, NULL, NULL);

    if (con_sock < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    char buffer[BUF_SIZE];

    while (1) {
        // Read command from console
        read_line(con_sock, buffer, BUF_SIZE);

        // Copy command to tokenize
        int com_len = strlen(buffer);
        char *command = malloc(com_len+1);
        strcpy(command, buffer);

        // Get command name
        char *tokenizer = " \n\t";
        char *com_name = strtok(command, tokenizer);

        // Command: add
        if (!strcmp(com_name, "add")) {
            // Get source and target info
            char *src_dir = strtok(NULL, tokenizer);
            char *tar_dir = strtok(NULL, tokenizer);
            struct sync_pair pair;

            // Console has already checked that format is correct
            sscanf(src_dir, "%255[^@]@%255[^:]:%d", pair.src_dir.dir, pair.src_dir.host, &(pair.src_dir.port));
            sscanf(tar_dir, "%255[^@]@%255[^:]:%d", pair.tar_dir.dir, pair.tar_dir.host, &(pair.tar_dir.port));

            // Connect to socket
            int src_sock;
            enum socket_error sock_err_code;

            sock_err_code = socket_client_connect(pair.src_dir.host, pair.src_dir.port, &src_sock, &err);
            if (sock_err_code != SOCKET_SUCCESS) {
                printf("Couldn't connect to %s:%d : %s\n", pair.src_dir.host, pair.src_dir.port, strerror(err));
                continue;
            }

            // Send LIST command
            snprintf(buffer, BUF_SIZE, "LIST %s ", pair.src_dir.dir);
            write_bytes(src_sock, buffer, strlen(buffer));

            // Read line by line
            read_line(src_sock, pair.file, FILE_SIZE);
            pair.file[strlen(pair.file)-1] = '\0';

            while (strcmp(pair.file, ".")) {
                // If file is already in buffer
                if (sync_buffer_file_exists(pair)) {
                    // Write error message
                    char datetime[DATETIME_SZ];
                    get_date_time(datetime, DATETIME_SZ);

                    snprintf(buffer, BUF_SIZE, "[%s] Already in queue: %s/%s@%s:%d\n",
                    datetime, pair.src_dir.dir, pair.file, pair.src_dir.host, pair.src_dir.port);

                // If file is not in buffer
                } else {
                    // Add to sync buffer
                    sync_buffer_place(pair);

                    char datetime[DATETIME_SZ];
                    get_date_time(datetime, DATETIME_SZ);

                    snprintf(buffer, BUF_SIZE, "[%s] Added file: %s/%s@%s:%d -> %s/%s@%s:%d\n",
                    datetime, pair.src_dir.dir, pair.file, pair.src_dir.host, pair.src_dir.port,
                    pair.tar_dir.dir, pair.file, pair.tar_dir.host, pair.tar_dir.port);

                    // Write to log file
                    logfile_write(log_file, buffer, strlen(buffer));
                }

                write_bytes(con_sock, buffer, strlen(buffer));
                printf("%s", buffer);

                // Read next line
                read_line(src_sock, pair.file, FILE_SIZE);
                pair.file[strlen(pair.file)-1] = '\0';
            }

            close(src_sock);
            write_bytes(con_sock, "\n", 1);

        // Command: cancel
        } else if (!strcmp(com_name, "cancel")) {
            char *src_dir_name = strtok(NULL, "\n");

            // Buffer for cancelled directories
            struct dir_location *deleted_buffer = malloc(sync_buffer_size() * sizeof(struct dir_location));
            if (deleted_buffer == NULL) {
                perror("Memory allocation failed");
                exit(EXIT_FAILURE);
            }

            // Remove from sync buffer
            if (!sync_buffer_cancel_dir(src_dir_name, deleted_buffer, sync_buffer_size())) {
                // If no file is removed
                char datetime[DATETIME_SZ];
                get_date_time(datetime, DATETIME_SZ);

                snprintf(buffer, BUF_SIZE, "[%s] Directory not being synchronized: %s\n", datetime, src_dir_name);
                
                write_bytes(con_sock, buffer, strlen(buffer));
                write_bytes(con_sock, "\n", 1);

                printf("%s", buffer);
                
                continue;
            }

            // Write to log file
            char datetime[DATETIME_SZ];
            get_date_time(datetime, DATETIME_SZ);

            // Print message for every directory that was cancelled
            for (int i = 0; i < sync_buffer_size(); i++) {
                if (deleted_buffer[i].dir[0] == '\0') break;

                snprintf(buffer, BUF_SIZE, "[%s] Synchronization stopped for %s@%s:%d\n",
                datetime, deleted_buffer[i].dir, deleted_buffer[i].host, deleted_buffer[i].port);
    
                write_bytes(con_sock, buffer, strlen(buffer));
                logfile_write(log_file, buffer, strlen(buffer));
                printf("%s", buffer);
            }

            free(deleted_buffer);
            write_bytes(con_sock, "\n", 1);

        } else if (!strcmp(com_name, "shutdown")) {
            // Write to log file
            char datetime[DATETIME_SZ];
            get_date_time(datetime, DATETIME_SZ);

            snprintf(buffer, BUF_SIZE, "[%s] Shutting down manager...\n[%s] Waiting for all active workers to finish.\n[%s] Processing remaining queued tasks.\n", 
            datetime, datetime, datetime);

            write_bytes(con_sock, buffer, strlen(buffer));
            printf("%s", buffer);

            sync_buffer_quit();
            break;
        }
    }

    *ret_con_sock = con_sock;
}

int nfs_join_worker_thread_pool(pthread_t *worker_pool, int worker_limit) {
    int err;
    for (int i = 0; i < worker_limit; i++) {
        err = pthread_join(worker_pool[i], NULL);
        if (err) return err;
    }

    return 0;
}

void nfs_send_shutdown_complete_message(int con_sock) {
    char datetime[DATETIME_SZ];
    get_date_time(datetime, DATETIME_SZ);

    char buffer[50];
    logfile_destroy(log_file);

    snprintf(buffer, 50, "[%s] Manager shutdown complete.\n", datetime);

    write_bytes(con_sock, buffer, strlen(buffer));
    write_bytes(con_sock, "\n", 1);
    printf("%s", buffer);
}

