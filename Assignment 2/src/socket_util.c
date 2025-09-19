#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "../include/socket_util.h"

enum socket_error socket_server_create(int port, int *ret_sock, int *err) {
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { *err = errno; return SOCKET; }

    *ret_sock = sock;

    // Bind socket to address
    struct sockaddr_in address;

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *) &address, sizeof(address)) < 0) 
    { *err = errno; return BIND; }

    // Listen for connections
    if (listen(sock, SOMAXCONN) < 0) { *err = errno; return LISTEN; }

    return SOCKET_SUCCESS;
}

enum socket_error socket_client_connect(char *hostname, int port, int *ret_sock, int *err) {
    
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { *err = errno; return SOCKET; }

    *ret_sock = sock;

    // Find address
    struct hostent *host = gethostbyname(hostname);
    if (host == NULL) { *err = errno; return GETHOSTBYNAME; }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
    address.sin_port = htons(port);

    // Initiate connection
    if (connect(sock, (struct sockaddr *) &address, sizeof(address)) < 0)
    { *err = errno; return CONNECT; }

    return SOCKET_SUCCESS;
}

void print_sock_err_message(enum socket_error sock_err_code, int err, char *hostname) {
    switch (sock_err_code) {
        case SOCKET:
            fprintf(stderr, "Failed to create socket: %s\n", strerror(err));
            break;
        case GETHOSTBYNAME:
            if (hostname != NULL) fprintf(stderr, "Failed to get host %s: %s\n", hostname, strerror(err));
            else fprintf(stderr, "Failed to get host: %s\n", strerror(err));
            break;
        case CONNECT:
            fprintf(stderr, "Failed to connect to socket: %s\n", strerror(err));
            break;
        case BIND:
            fprintf(stderr, "Failed to bind socket: %s\n", strerror(err));
            break;
        case LISTEN:
            fprintf(stderr, "Listen failed: %s\n", strerror(err));
            break;
        default:
            fprintf(stderr, "An unknown errored occured with socket.\n");
            break;
    }
}