#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../include/util.h"

#define BUF_SIZE 1024

char *con_in = "fss_out";
char *con_out = "fss_in";

extern char *optarg;

int main(int argc, char *argv[]) {
    char *logfile_name = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "l:")) != -1) {
        switch(opt) {
            case 'l':
                logfile_name = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s -l <console_logfile>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (logfile_name == NULL) {
        fprintf(stderr, "Usage: %s -l <console_logfile>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Open log file for writiing
    FILE *log_file = fopen(logfile_name, "w");

    if (log_file == NULL) {
        perror("Couldn't open log file");
        exit(EXIT_FAILURE);
    }

    // Open pipe communication
    int con_in_fd = open(con_in, O_RDWR);
    int con_out_fd = open(con_out, O_RDWR);

    if (con_in_fd < 0 || con_out_fd < 0) {
        perror("Couldn't open pipes");
        exit(EXIT_FAILURE);
    }

    char buffer[BUF_SIZE];
    char datetime[20];
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

        // Check if command has correct format and print it to log file
        if (!strcmp(com_name, "add")) {
            char *src_dir = strtok(NULL, tokenizer);
            char *tar_dir = strtok(NULL, tokenizer);

            if (src_dir == NULL || tar_dir == NULL || strtok(NULL, tokenizer) != NULL) {
                fprintf(stderr, "Invalid command! Try: add <source> <target>\n");
                free(command); continue;
            }

            get_date_time(datetime, sizeof(datetime));
            fprintf(log_file, "[%s] Command add %s -> %s\n", datetime, src_dir, tar_dir);
            fflush(log_file);

        } else if (!strcmp(com_name, "status") || !strcmp(com_name, "cancel") || !strcmp(com_name, "sync")) {
            char *dir = strtok(NULL, tokenizer);

            if (dir == NULL || strtok(NULL, tokenizer) != NULL) {
                fprintf(stderr, "Invalid command! Try: %s <directory>\n", com_name);
                free(command); continue;
            }

            get_date_time(datetime, sizeof(datetime));
            fprintf(log_file, "[%s] Command %s %s\n", datetime, com_name, dir);
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

        } else {
            fprintf(stderr, "Invalid command!\n");
            free(command); continue;
        }

        free(command);

        // Write to manager
        write_bytes(con_out_fd, buffer, com_len);

        // Read result
        read_line(con_in_fd, buffer, BUF_SIZE);
        int num_of_lines = atoi(buffer);

        for (int i = 0; i < num_of_lines; i++) {
            read_line(con_in_fd, buffer, BUF_SIZE);
            fprintf(stdout, "%s", buffer); fflush(stdout);
        }

        // Shutdown
        if (shutdown) {
            break;
        }
    }

    close(con_in_fd);
    close(con_out_fd);
    fclose(log_file);
    exit(EXIT_SUCCESS);
}
