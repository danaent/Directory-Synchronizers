#define DATETIME_SZ 20

// Returns size of file or -1 in case of error
ssize_t file_size(char *file);

// Reads from fd and writes to buf until EOF is reached or until nbytes have been read
// Not affected by signal interrupts
// Returns number of bytes read or -1 in case of error
ssize_t read_eof(int fd, char *buf, ssize_t nbytes);

// Reads from fd to buf until a newline character is reached or nbytes-1 characters have been read
// Terminates with a NULL character after newline
// Returns number of bytes read or -1 in case of error
ssize_t read_line(int fd, char *buf, ssize_t nbytes);

// Reads from fd to buf until a white character is reached or nbytes-1 characters have been read
// Replaces white character with a NULL terminator
// Returns number of bytes read or -1 in case of error
ssize_t read_wc(int fd, char *buf, ssize_t nbytes);

// Reads exactly nbytes from fd to buf. Not affected by signal interrupts.
// Returns 0 for success or errno if an error occurs
int read_bytes(int fd, char *buf, ssize_t nbytes);

// Writes nbytes from buf to fd. Not affected by signal interrupts.
// Returns 0 for success or errno if an error occurs
int write_bytes(int fd, char *buf, ssize_t nbytes);

// Write date and time into buffer in format "%Y-%m-%d %H:%M:%S"
// Buffer must be at least 20 characters long
// In case of error, "----Unknown time----" is written into buffer
// Returns 0 for success, -1 for failure
int get_date_time(char *buffer, size_t size);