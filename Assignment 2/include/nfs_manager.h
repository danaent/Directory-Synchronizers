#include <pthread.h>

// Creates a thread array of size worker_limit and returns it. It must be
// freed by main program.
// All threads execute the same worker function
// Returns NULL if malloc fails
// If thread creation fails, err is set to errno, otherwise it's set to 0
pthread_t *nfs_create_worker_thread_pool(int worker_limit, int *err);

// Reads all entries from config file config_name, sends LIST command to
// nfs_clients and adds all returned files from directories to sync buffer
// Also creates a log file in path log_name and writes log messages
void nfs_read_config_file(char *config_name, char *log_name);

// Connects to console in port and reads and executes commands
// Writes log messages to log file created by nfs_read_config_file
// Socket that communicates with console is returned in ret_con_sock.
// It must be closed before the program terminates.
void nfs_connect_to_console(int sock, int *ret_con_sock);

// Joins threads from worker_pool to main thread
// Returns 0 for success, errno for error
int nfs_join_worker_thread_pool(pthread_t *worker_pool, int worker_limit);

// Must be called after all threads have been terminated. Writes final
// shutdown message to console and stdout and closes log file.
void nfs_send_shutdown_complete_message(int con_sock);