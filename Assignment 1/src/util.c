#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include "../include/util.h"

#define BUF_SIZE 1024


char *file_name_concat(char *dir, char *file) {
    char *final = malloc((strlen(dir) + strlen(file) + 2) * sizeof(char));
            
    if (final == NULL) {
        return NULL;
    }

    strcpy(final, dir);
    strcat(final, "/");
    strcat(final, file);

    return final;
}

enum file_management_error file_copy(char *src, char *tar, int *err_file) {
    // Open source and target files
    int src_fd = open(src, O_RDONLY);

    if (src_fd < 0) {
        *err_file = 0; return OPEN_FAILED;
    }

    int tar_fd = open(tar, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (tar_fd < 0) {
        *err_file = 1; return OPEN_FAILED;
    }

    // Initialize buffer 
    char buffer[BUF_SIZE];

    // Copy data
    ssize_t nread, nwrite;
    while ((nread = read_eof(src_fd, buffer, BUF_SIZE)) > 0) {
        nwrite = write_bytes(tar_fd, buffer, nread);
        if (nwrite < 0) break;
    }

    if (nread < 0) {
        *err_file = 0; return READ_FAILED;
    }

    if (nwrite < 0) {
        *err_file = 1; return WRITE_FAILED;
    }

    // Close files
    close(src_fd);
    close(tar_fd);

    return SUCCESS;
}

ssize_t read_eof(int fd, char *buf, ssize_t nbytes)
{
    ssize_t bytes_read = 0;  // Bytes read so far
    ssize_t bytes;           // Bytes returned on each read

    // Read until all bytes have been read
    while ((bytes = read(fd, buf + bytes_read, nbytes)) != 0)
    {
        // If an error occurs
        if (bytes < 0) {
            // If error is not a signal interrupt
            if (errno != EINTR)
                return -1;
            
            continue;
        }
        
        bytes_read += bytes; // Increase bytes that have been read
        nbytes -=bytes;      // Decrease bytes left
    }

    return bytes_read;
}

ssize_t read_line(int fd, char *buf, ssize_t nbytes) {

    ssize_t bytes;           // Bytes returned on each read

    for (ssize_t i = 0; i < nbytes-1; i++) {
        bytes = read(fd, buf + i, 1); // Read one character

        if (bytes <= 0) {
            if (errno != EINTR)
                return -1;
            
            i--;
            continue;
        }

        if (buf[i] == '\n') {
            buf[i+1] = '\0';
            return i+1;
        }
    }

    return -1;
}

ssize_t write_bytes(int fd, char *buf, ssize_t nbytes)
{
    ssize_t bytes_written = 0;  // Bytes written so far
    ssize_t bytes;              // Bytes returned on each write

    // Write until all bytes have been written
    while ((bytes = write(fd, buf + bytes_written, nbytes)) != nbytes)
    {
        // If an error occurs
        if (bytes < 0) {
            // If error is not a signal interrupt
            if (errno != EINTR)
                return -1;
            
            continue;
        }
        
        bytes_written += bytes; // Increase bytes that have been written
        nbytes -=bytes;         // Decrease bytes left
    }

    return bytes_written;
}

int get_date_time(char *buffer, size_t size) {
    time_t t = time(NULL);

    if (t < 0 || size < 20) {
        strcpy(buffer, "----Unknown time----");
        return -1;
    }

    struct tm *tmp = localtime(&t);
    if (tmp == NULL) {
        strcpy(buffer, "----Unknown time----");
        return -1;
    }

    if (strftime(buffer, size, "%Y-%d-%m %X", tmp) == 0) {
        strcpy(buffer, "----Unknown time----");
        return -1;
    }

    return 0;
}



