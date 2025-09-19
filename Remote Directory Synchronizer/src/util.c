#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include "../include/util.h"

#define BUF_SIZE 1024

ssize_t file_size(char *file)
{
    struct stat buf;

    if (stat(file, &buf) < 0)
        return -1;

    return buf.st_size;
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
            return i;
        }
    }

    return -1;
}

ssize_t read_wc(int fd, char *buf, ssize_t nbytes) {

    ssize_t bytes;           // Bytes returned on each read

    for (ssize_t i = 0; i < nbytes-1; i++) {
        bytes = read(fd, buf + i, 1); // Read one character

        if (bytes <= 0) {
            if (errno != EINTR)
                return -1;
            
            i--;
            continue;
        }

        if (buf[i] == '\n' || buf[i] == ' ' || buf[i] == '\t') {
            buf[i] = '\0';
            return i;
        }
    }

    return -1;
}

int read_bytes(int fd, char *buf, ssize_t nbytes)
{
    ssize_t bytes_read = 0;  // Bytes read so far
    ssize_t bytes;           // Bytes returned on each read

    // Read until all bytes have been read
    while ((bytes = read(fd, buf + bytes_read, nbytes)) != nbytes)
    {
        // If an error occurs
        if (bytes < 0) {
            // If error is not a signal interrupt
            if (errno != EINTR)
                return errno;
            
            continue;
        }
        
        bytes_read += bytes; // Increase bytes that have been read
        nbytes -=bytes;      // Decrease bytes left
    }

    return 0;
}

int write_bytes(int fd, char *buf, ssize_t nbytes)
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
                return errno;
            
            continue;
        }
        
        bytes_written += bytes; // Increase bytes that have been written
        nbytes -=bytes;         // Decrease bytes left
    }

    return 0;
}

int get_date_time(char *buffer, size_t size) {
    time_t t = time(NULL);

    if (t < 0) {
        strcpy(buffer, "----Unknown time----");
        return -1;
    }

    struct tm *tmp = localtime(&t);
    if (tmp == NULL) {
        strcpy(buffer, "----Unknown time----");
        return -1;
    }

    if (strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tmp) == 0) {
        strcpy(buffer, "----Unknown time----");
        return -1;
    }

    return 0;
}



