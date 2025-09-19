#define FILE_SIZE 256
#define DIR_SIZE 256
#define HOST_SIZE 256

// Struct for directory of the form dir@host:port
struct dir_location {
    char dir[DIR_SIZE];
    char host[HOST_SIZE];
    int port;
};

// File in src_dir will be synced to file of the same name in tar_dir
struct sync_pair {
    char file[FILE_SIZE];
    struct dir_location src_dir;
    struct dir_location tar_dir;
};

// Initializes cyclical buffer with size slots
int sync_buffer_init(int size);

// Returns size of buffer
int sync_buffer_size(void);

// Returns number of items in buffer
int sync_buffer_count(void);

// Places a pair in buffer, suspends if buffer is full
void sync_buffer_place(struct sync_pair pair);

// Removes a pair from buffer and writes it to pair, suspends if buffer is empty
// If sync_buffer_quit has been called, obtain doesn't stop if there are still items
// in the buffer
// If sync_buffer_quit has been called and a thread is suspended on obtain, it will
// stop being suspended and exit the function
void sync_buffer_obtain(struct sync_pair *pair);

// Return 1 if source file from pair exists in the buffer, 0 otherwise
int sync_buffer_file_exists(struct sync_pair pair);

// Remove all pairs with src_dir equal to src_dir_name from buffer
// Adds info about cancelled directories to deleted_buffer of size buffer_size for logging
// Buffer gets NULL terminated
// Return 1 if any pairs were deleted, 0 otherwise
int sync_buffer_cancel_dir(char *src_dir_name, struct dir_location *deleted_buffer, int buffer_size);

// Set quit variable and signal threads waiting on obtain to stop
void sync_buffer_quit(void);

// Free resources for cyclical buffer
void sync_buffer_destroy(void);