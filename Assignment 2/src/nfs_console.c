#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "../include/socket_util.h"
#include "../include/util.h"

#define BUF_SIZE 1024

extern char *optarg;

int main(int argc, char *argv[]) {
    char *logfile_name = NULL;
    char *man_host = NULL;
    int man_port = -1;

    int opt;
    while ((opt = getopt(argc, argv, "l:h:p:")) != -1) {
        switch(opt) {
            case 'l':
                logfile_name = optarg;
                break;
            case 'h':
                man_host = optarg;
                break;
            case 'p':
                man_port = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -l <console_logfile> -h <host_IP> -p <host_port>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (logfile_name == NULL || man_host == NULL) {
        fprintf(stderr, "Usage: %s -l <console_logfile> -h <host_IP> -p <host_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (man_port < 0) {
        fprintf(stderr, "Please give a valid port number.\n");
        exit(EXIT_FAILURE);
    }

    // Open log file for writiing
    FILE *log_file = fopen(logfile_name, "w");

    if (log_file == NULL) { 
        perror("Failed to open logfile");
        exit(EXIT_FAILURE);
    }

    int man_sock;
    enum socket_error sock_err_code; int err;

    sock_err_code = socket_client_connect(man_host, man_port, &man_sock, &err);
    if (sock_err_code != SOCKET_SUCCESS) {
        print_sock_err_message(sock_err_code, err, man_host);
        exit(EXIT_FAILURE);
    }

    char buffer[BUF_SIZE];
    char datetime[DATETIME_SZ];
    char *tokenizer = " \n\t";
    int shutdown = 0;

    // Read from stdin line by line
    while (fgets(buffer, BUF_SIZE, stdin)) {
        int com_len = strlen(buffer);

        // Copy command to tokenize
        char *command = malloc(com_len+1);
        strcpy(command, buffer);

        // Get name of command
        char *com_name = strtok(command, tokenizer);

        // Check if command has correct format
        if (!strcmp(com_name, "add")) {
            char *src_dir = strtok(NULL, tokenizer);
            char *tar_dir = strtok(NULL, tokenizer);

            if (src_dir == NULL || tar_dir == NULL || strtok(NULL, tokenizer) != NULL) {
                fprintf(stderr, "Invalid command! Try: add <source> <target>\n");
                free(command); continue;
            }

            char dir[256], host[256]; int port;
            if (sscanf(src_dir, "%255[^@]@%255[^:]:%d", dir, host, &port) != 3
                || sscanf(tar_dir, "%255[^@]@%255[^:]:%d", dir, host, &port) != 3) {

                fprintf(stderr, "Directories must have the format: dir@host:port\n");
                free(command); continue;
            }

            // Write to log file
            get_date_time(datetime, sizeof(datetime));
            fprintf(log_file, "[%s] Command add %s -> %s\n", datetime, src_dir, tar_dir);
            fflush(log_file);

        } else if (!strcmp(com_name, "cancel")) {
            char *token = strtok(NULL, tokenizer);

            if (token == NULL || strtok(NULL, tokenizer) != NULL) {
                fprintf(stderr, "Invalid command! Try: %s <directory>\n", com_name);
                free(command); continue;
            }

            get_date_time(datetime, sizeof(datetime));
            fprintf(log_file, "[%s] Command cancel %s\n", datetime, token);
            fflush(log_file);

        } else if (!strcmp(com_name, "shutdown")) {
            if (strtok(NULL, tokenizer) != NULL) {
                fprintf(stderr, "Invalid command! Try: shutdown\n");
                free(command); continue;
            }

            get_date_time(datetime, sizeof(datetime));
            fprintf(log_file, "[%s] Command shutdown\n", datetime);
            fflush(log_file);

            shutdown = 1;
        }

        free(command);
        write_bytes(man_sock, buffer, strlen(buffer));

        // Read output from manager
        read_line(man_sock, buffer, BUF_SIZE);

        while (strcmp(buffer, "\n")) {
            printf("%s", buffer);
            read_line(man_sock, buffer, BUF_SIZE);
        }

        if (shutdown) break;
    }

    close(man_sock);
    fclose(log_file);
    exit(EXIT_SUCCESS);
}