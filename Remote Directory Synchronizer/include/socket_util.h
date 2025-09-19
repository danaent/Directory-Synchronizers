// Error codes for functions
enum socket_error {SOCKET_SUCCESS, SOCKET, GETHOSTBYNAME, CONNECT, BIND, LISTEN};

// Creates a socket, binds it to any address on port and listens for connections
// New socket is returned in ret_sock
// If an error code is returned, err is set to errno
enum socket_error socket_server_create(int port, int *ret_sock, int *err);

// Creates a socket and connects to hostname:port
// New socket is returned in ret_sock
// If an error code is returned, err is set to errno
enum socket_error socket_client_connect(char *hostname, int port, int *ret_sock, int *err);

// Prints an error message to stderr specific to sock_err_code using strerror with err
// If hostname is not NULL, it may be used in printed message
void print_sock_err_message(enum socket_error sock_err_code, int err, char *hostname);