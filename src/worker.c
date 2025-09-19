#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include "../include/util.h"

#define ERR_BUF_SIZE_DEFAULT 4096
#define BUF_SIZE 1024

// Struct used for error reporting
struct error_buffer {
    char *buffer;
    int size;        // Current size of buffer - buffer is reallocated if needed
    int pos;         // Last written byte of buffer
} error_buffer;

// Function prototypes
int write_to_err_buf(struct error_buffer error_buffer, char *file, char *func);
void report_status_success(int files_processed);
void report_status_error(struct error_buffer error_buffer);
void report_status_partial(struct error_buffer error_buffer, int files_processed, int files_failed);
void report_irrecoverable_error(char *error, int use_errno);

int main(int argc, char *argv[]) {

    // Initialize error buffer
    error_buffer.size = ERR_BUF_SIZE_DEFAULT;
    error_buffer.pos = 0;
    error_buffer.buffer = malloc(error_buffer.size * sizeof(char));

    if (error_buffer.buffer == NULL) {
        report_irrecoverable_error("malloc failed", 1);
        exit(EXIT_FAILURE);
    }

    // Check argument count
    if (argc != 5) {
        report_irrecoverable_error("Wrong number of arguments", 0);
        free(error_buffer.buffer);
        exit(EXIT_FAILURE);
    }

    // Get arguments
    char *src_dir_name = argv[1];
    char *tar_dir_name = argv[2];
    char *op_str = argv[4];
    char *filename = !strcmp(op_str, "FULL")? "ALL": argv[3];

    int files_processed = 0;
    int files_failed = 0;

    enum file_management_error err_num; // Used for error handling when copying files
    int err_file;                       // Indicates which file error occured in - 0 for source, 1 for target

    // OPERATION: FULL
    if (!strcmp(op_str, "FULL")) {

        // Open source and target directories
        DIR *src_dir = opendir(src_dir_name); 

        if (src_dir == NULL) {
            write_to_err_buf(error_buffer, src_dir_name, "opendir failed");
            report_status_error(error_buffer); free(error_buffer.buffer);
            exit(EXIT_FAILURE);
        }

        DIR *tar_dir = opendir(tar_dir_name);

        if (tar_dir == NULL) {
            write_to_err_buf(error_buffer, tar_dir_name, "opendir failed");
            report_status_error(error_buffer); free(error_buffer.buffer);
            closedir(src_dir);
            exit(EXIT_FAILURE);
        }

        // Go through directory
        struct dirent *src_dir_ent;
        while (1) {
            errno = 0;
            src_dir_ent = readdir(src_dir);
            
            if (src_dir_ent == NULL) {
                if (!errno) break; // All entities have been read

                write_to_err_buf(error_buffer, src_dir_name, "readdir failed");
                report_status_error(error_buffer); free(error_buffer.buffer);
                exit(EXIT_FAILURE);
            }

            // Skip unnecessary files
            if (src_dir_ent->d_ino == 0 || !strcmp(src_dir_ent->d_name, ".") || !strcmp(src_dir_ent->d_name, "..")) continue;

            // Create name of source file
            char *src_file_name = file_name_concat(src_dir_name, src_dir_ent->d_name);
            
            if (src_file_name == NULL) {
                report_irrecoverable_error("malloc failed", 1); 
                free(error_buffer.buffer);
                closedir(src_dir); closedir(tar_dir);
                exit(EXIT_FAILURE);
            }

            // Create name of new file in target directory
            char *tar_file_name = file_name_concat(tar_dir_name, src_dir_ent->d_name);

            if (tar_file_name == NULL) {
                report_irrecoverable_error("malloc failed", 1); 
                free(error_buffer.buffer); free(src_file_name);
                closedir(src_dir); closedir(tar_dir);
                exit(EXIT_FAILURE);
            }

            // Copy source to target
            err_num = file_copy(src_file_name, tar_file_name, &err_file);

            if (err_num == SUCCESS) {
                free(src_file_name); free(tar_file_name);
                files_processed++;
                continue;
            }
            
            // Check error
            int check_alloc;
            switch (err_num) {
                case OPEN_FAILED:
                    check_alloc = write_to_err_buf(error_buffer, err_num? tar_file_name: src_file_name, "open failed");
                    break;
                case READ_FAILED:
                    check_alloc = write_to_err_buf(error_buffer, src_file_name, "read failed");
                    break;
                case WRITE_FAILED:
                    check_alloc = write_to_err_buf(error_buffer, tar_file_name, "write failed");
                    break;
                default:
                    check_alloc = write_to_err_buf(error_buffer, err_num? tar_file_name: src_file_name, "unknown failure");
                    break;
            }

            if (check_alloc < 0) {
                report_irrecoverable_error("malloc failed", 1);
                closedir(src_dir); closedir(tar_dir);
                free(src_file_name); free(tar_file_name);
            }

            free(src_file_name); free(tar_file_name);
            files_failed++;
        }

        closedir(src_dir); closedir(tar_dir);

    // OPERATION: ADDED or OPERATION: MODIFIED
    } else if (!strcmp(op_str, "ADDED") || !strcmp(op_str, "MODIFIED")) {
        // Create name of source file
        char *src_file_name = file_name_concat(src_dir_name, filename);
            
        if (src_file_name == NULL) {
            report_irrecoverable_error("malloc failed", 1);
            free(error_buffer.buffer);
            exit(EXIT_FAILURE);
        }

        // Create name of file in target directory
        char *tar_file_name = file_name_concat(tar_dir_name, filename);

        if (tar_file_name == NULL) {
            report_irrecoverable_error("malloc failed", 1);
            free(error_buffer.buffer); free(src_file_name);
            exit(EXIT_FAILURE);
        }

        // Copy source to target
        err_num = file_copy(src_file_name, tar_file_name, &err_file);

        if (err_num == SUCCESS) {
            files_processed++;
            free(src_file_name); free(tar_file_name);
        } else { // Check error
            switch (err_num) {
                case OPEN_FAILED:
                    write_to_err_buf(error_buffer, err_num? tar_file_name: src_file_name, "open failed");
                    break;
                case READ_FAILED:
                    write_to_err_buf(error_buffer, src_file_name, "read failed");
                    break;
                case WRITE_FAILED:
                    write_to_err_buf(error_buffer, tar_file_name, "write failed");
                    break;
                default:
                    write_to_err_buf(error_buffer, err_num? tar_file_name: src_file_name, "unknown failure");
                    break;
            }
            free(src_file_name); free(tar_file_name);
            report_status_error(error_buffer); free(error_buffer.buffer);
            exit(EXIT_FAILURE);
        }

    } else if (!strcmp(op_str, "DELETED")) {
        // Create name of file in target directory
        char *tar_file_name = file_name_concat(tar_dir_name, filename);

        if (tar_file_name == NULL) {
            write_to_err_buf(error_buffer, filename, "malloc failed");
            report_status_error(error_buffer); free(error_buffer.buffer);
            exit(EXIT_FAILURE);
        }

        // Delete file
        if (unlink(tar_file_name) < 0) {
            write_to_err_buf(error_buffer, tar_file_name, "unlink failed");
            report_status_error(error_buffer); free(error_buffer.buffer);
            free(tar_file_name);
            exit(EXIT_FAILURE);
        }
        
        free(tar_file_name);
        files_processed++;
    }

    if (!files_failed) {
        report_status_success(files_processed);
        free(error_buffer.buffer);
        exit(EXIT_SUCCESS);
    }

    if (!files_processed) {
        report_status_error(error_buffer);
        free(error_buffer.buffer);
        exit(EXIT_FAILURE);
    }

    report_status_partial(error_buffer, files_processed, files_failed);
    free(error_buffer.buffer);
    exit(EXIT_SUCCESS);
}

// Writes a line to error_buffer indicating an error while using func for file
// The line follows the format: -File: <file> - <func>: <error>
// <error> is taken from errno
int write_to_err_buf(struct error_buffer error_buffer, char *file, char *func) {
    char *error_str = strerror(errno);
    int error_mes_len = 14+strlen(file)+strlen(error_str)+strlen(func);

    // Resize buffer if needed
    while (error_mes_len > error_buffer.size-error_buffer.pos) {
        error_buffer.buffer = realloc(error_buffer.buffer, error_buffer.size * 1.5 * sizeof(char));
        if (error_buffer.buffer == NULL) {
            return -1;
        }
    }
    
    // Write to buffer
    snprintf(error_buffer.buffer + error_buffer.pos, error_mes_len, "-File: %s - %s: %s\n", file, func, error_str);

    // Move positition
    error_buffer.pos += error_mes_len-1;
    return 0;
}

// Write successful report to stdout
void report_status_success(int files_processed) {
    char *report = "EXEC_REPORT_START\nSTATUS: SUCCESS\nDETAILS: %d files copied\nEXEC_REPORT_END\n";

    int buffer_len = strlen(report) + 100;
    char buffer[buffer_len];

    snprintf(buffer, buffer_len, report, files_processed);
    write_bytes(STDOUT_FILENO, buffer, strlen(buffer));
}

// Write error report to stdout
void report_status_error(struct error_buffer error_buffer) {
    char *report_start = "EXEC_REPORT_START\nSTATUS: ERROR\nDETAILS: 0 files copied\nERRORS:\n";
    char *report_end = "EXEC_REPORT_END\n";

    write_bytes(STDOUT_FILENO, report_start, strlen(report_start));
    write_bytes(STDOUT_FILENO, error_buffer.buffer, strlen(error_buffer.buffer));
    write_bytes(STDOUT_FILENO, report_end, strlen(report_end));
}

// Write partial report to stdout
void report_status_partial(struct error_buffer error_buffer, int files_processed, int files_failed) {
    char report_start[200];
    snprintf(report_start, 200, "EXEC_REPORT_START\nSTATUS: PARTIAL\nDETAILS: %d files copied, %d files skipped\nERRORS:\n", files_processed, files_failed);

    char *report_end = "EXEC_REPORT_END\n";

    write_bytes(STDOUT_FILENO, report_start, strlen(report_start));
    write_bytes(STDOUT_FILENO, error_buffer.buffer, strlen(error_buffer.buffer));
    write_bytes(STDOUT_FILENO, report_end, strlen(report_end));
}

// Write irrecoverable error report to stdout
// This happens when the worker fails unexpectedly because of a function call
// and has to be stopped immediately
// It is only used when malloc fails or when the number of arguments is wrong
// Issue is printed in ERRORS section of report
// If use errno is set to 1, errno is also printed as a string
void report_irrecoverable_error(char *issue, int use_errno) {
    char *error = strerror(errno);
    char *report_start = "EXEC_REPORT_START\nSTATUS: ERROR\nDETAILS: Worker failed\nERRORS:\n";
    char *report_end = "EXEC_REPORT_END\n";

    char message[200];

    if (use_errno)
        snprintf(message, 200, "%s: %s\n", issue, error);
    else
        strcpy(message, issue);

    write_bytes(STDOUT_FILENO, report_start, strlen(report_start));
    write_bytes(STDOUT_FILENO, message, strlen(message));
    write_bytes(STDOUT_FILENO, report_end, strlen(report_end));
}