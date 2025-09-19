#include <stdio.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <string.h> // strtok 
#include <dirent.h> // readdir
#include <errno.h>
#include "../include/util.h"
#include <unistd.h> // read
#include <fcntl.h> // open
#include "../include/socket_util.h"

#define BUF_SIZE 1024
char buffer[BUF_SIZE];

int execute_command(char *str, int sock);
int command_list(char *src_name, int sock);
int command_pull(char *file_name, int sock);
int command_push(char *file_name, int chunk_size, int sock);

int main(int argc, char *argv[]) {

    // Get port number
    if (argc != 3 || strcmp(argv[1], "-p")) {
        fprintf(stderr, "Usage: %s -p <port_number\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[2]);

    // Create socket
    int sock;
    enum socket_error sock_err_code; int err;

    sock_err_code = socket_server_create(port, &sock, &err);
    if (sock_err_code != SOCKET_SUCCESS) {
        print_sock_err_message(sock_err_code, err, NULL);
        exit(EXIT_FAILURE);
    }

    while (1) {
        // Accept connection
        int newsock = accept(sock, NULL, NULL);

        if (newsock < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Read and execute command
        read_wc(newsock, buffer, BUF_SIZE);
        execute_command(buffer, newsock);

        close(newsock);
    }
}

// // Execute command written in command string, write result to socket sock
int execute_command(char *command, int sock) {

    if (!strcmp(command, "LIST")) {
        read_wc(sock, buffer, BUF_SIZE);
        char *src_name = malloc(strlen(buffer)+1);
        strcpy(src_name, buffer);

        return command_list(src_name, sock);
    } else if (!strcmp(command, "PULL")) {
        read_wc(sock, buffer, BUF_SIZE);
        char *file_name = malloc(strlen(buffer)+1);
        strcpy(file_name, buffer);

        return command_pull(file_name, sock);
    } else if (!strcmp(command, "PUSH")) {
        read_wc(sock, buffer, BUF_SIZE);
        char *file_name = malloc(strlen(buffer)+1);
        strcpy(file_name, buffer);

        read_wc(sock, buffer, BUF_SIZE);
        char *chunk_size_str = malloc(strlen(buffer)+1);
        strcpy(chunk_size_str, buffer);

        int chunk_size = atoi(chunk_size_str);

        return command_push(file_name, chunk_size, sock);
    } else {
        fprintf(stderr, "Command %s does not exist\n", command);
        return -1;
    }
}

// Execute LIST command for directory src_name, write result in socket sock
// Return 0 for success, -1 for error
int command_list(char *src_name, int sock) {

    // Add . at the start of the name to read it correctly
    char *src_name_old = src_name;

    src_name = malloc(strlen(src_name) + 2);
    if (src_name == NULL) {
        perror("malloc");
        write_bytes(sock, ".\n", 2);
        return -1;
    }

    src_name[0] = '.';
    strcpy(src_name+1, src_name_old);

    // Open source directory
    DIR *src_dir = opendir(src_name);
    
    // If directory doesn't exist, return . to treat it as an empty directory
    if (src_dir == NULL) {
        write_bytes(sock, ".\n", 2);
        return 0;
    }

    // Go through directory
    struct dirent *src_dir_ent;
    while (1) {
        src_dir_ent = readdir(src_dir);
        
        if (src_dir_ent == NULL) {
            // All entities have been read
            // Write terminating character (.)
            write_bytes(sock, ".\n", 2);
            break;
        }

        // Skip unnecessary files
        if (src_dir_ent->d_ino == 0 || !strcmp(src_dir_ent->d_name, ".") || !strcmp(src_dir_ent->d_name, "..")) continue;

        // Write file name + newline to socket
        write_bytes(sock, src_dir_ent->d_name, strlen(src_dir_ent->d_name));
        write_bytes(sock, "\n", 1);
    }

    return 0;
}

// Execute PULL command for file file_name, write result in socket sock
// Return 0 for success, -1 for error
int command_pull(char *file_name, int sock) {
    // Add . at the start of the name to read it correctly
    char *file_name_old = file_name;

    file_name = malloc(strlen(file_name) + 2);
    if (file_name == NULL) {
        int err_num = errno;

        int32_t size = -1;
        uint32_t network_size = htonl(size);
        write_bytes(sock, (char *) &network_size, sizeof(network_size));

        snprintf(buffer, BUF_SIZE, " Memory allocation failed - %s\n", strerror(err_num));
        write_bytes(sock, buffer, strlen(buffer));

        perror("malloc");
        return -1;
    }

    file_name[0] = '.';
    strcpy(file_name+1, file_name_old);

    // Get file size
    int32_t size = file_size(file_name);

    // If stat fails write error to socket
    if (size < 0) {
        int err_num = errno;

        size = -1;
        uint32_t network_size = htonl(size);
        write_bytes(sock, (char *) &network_size, sizeof(network_size));

        snprintf(buffer, BUF_SIZE, " File: %s - %s\n", file_name, strerror(err_num));
        write_bytes(sock, buffer, strlen(buffer));
        
        perror("stat");
        return -1;
    }

    // Open file for reading
    int fd = open(file_name, O_RDONLY);

    // If open fails, write error to socket
    if (fd < 0) {
        int err_num = errno;

        size = -1;
        uint32_t network_size = htonl(size);
        write_bytes(sock, (char *) &network_size, sizeof(network_size));

        snprintf(buffer, BUF_SIZE, " File: %s - %s\n", file_name, strerror(err_num));
        write_bytes(sock, buffer, strlen(buffer));

        fprintf(stderr, "Failed to open %s: %s\n", file_name, strerror(err_num));
        return -1; 
    }

    // Write file size to socket
    uint32_t network_size = htonl(size);
    write_bytes(sock, (char *) &network_size, sizeof(network_size));

    // Write white character
    write_bytes(sock, " ", 1);

    // Copy data to socket
    ssize_t nread;
    while ((nread = read_eof(fd, buffer, BUF_SIZE)) > 0) {
        write_bytes(sock, buffer, nread);
    }

    // Close file descriptor
    close(fd);
    return 0;
}

// Execute PUSH command for file file_name, write result in socket sock
// Return 0 for success, -1 for error
int command_push(char *file_name, int chunk_size, int sock) {
    // Add . at the start of the name to read it correctly
    char *file_name_old = file_name;

    file_name = malloc(strlen(file_name) + 2);
    if (file_name == NULL) {
        perror("malloc"); return -1;
    }

    file_name[0] = '.';
    strcpy(file_name+1, file_name_old);

    if (chunk_size == -1) { // If chunk size is -1, create/truncate file
        int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) { 
            fprintf(stderr, "Failed to open %s: %s\n", file_name, strerror(errno));
            return -1; 
        }

        close(fd);
        return 0;
    }

    // Open file for appending
    int fd = open(file_name, O_WRONLY | O_APPEND);
    if (fd < 0) { 
        fprintf(stderr, "Failed to open %s: %s\n", file_name, strerror(errno));
        return -1; 
    }

    // Copy data from socket to file

    // Bytes left to read in socket. At first, there are chunk_size bytes left
    ssize_t bytes_left = chunk_size;

    // Bytes to read in this iteration. No more than BUF_SIZE bytes can be read at once.
    ssize_t bytes_to_read = bytes_left < BUF_SIZE ? bytes_left : BUF_SIZE;

    while (bytes_left != 0) {
        read_bytes(sock, buffer, bytes_to_read);
        write_bytes(fd, buffer, bytes_to_read);

        bytes_left -= bytes_to_read;
        bytes_to_read = bytes_left < BUF_SIZE ? bytes_left : BUF_SIZE;
    }

    // Close file
    close(fd);
    return 0;
}



