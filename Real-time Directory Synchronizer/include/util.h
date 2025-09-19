#define DATETIME_SZ 20

enum file_management_error {SUCCESS, OPEN_FAILED, READ_FAILED, WRITE_FAILED};

// Performs string concatenation of dir + "/" + file
// Returns pointer to concatenated string
// Memory allocation is performed, so final string must be freed afterwards
// Returns NULL if memory allocation fails
char *file_name_concat(char *dir, char *file);

// Copies contents of file src to file tar, creates tar if it doesn't exist
// Returns SUCCESS or the type of error occured
// err_file is set to 0 if the error was in src and 1 if the error was in tar
enum file_management_error file_copy(char *src, char *tar, int *err_file);

// Reads from fd and writes to buf until EOF is reached or until nbytes have been read
// Not affected by signal interrupts
// Returns number of bytes read or -1 in case of error
ssize_t read_eof(int fd, char *buf, ssize_t nbytes);

// Reads from fd to buf until a newline character is reached or nbytes-1 characters have been read
// Terminates with a NULL character after newline
// Returns number of bytes read or -1 in case of error
ssize_t read_line(int fd, char *buf, ssize_t nbytes);

// Writes all nbytes of buf to fd. Not affected by signal interrupts.
// Returns number of bytes written or -1 in case of error
ssize_t write_bytes(int fd, char *buf, ssize_t nbytes);

// Write date and time into buffer in format "%Y-%d-%m %X"
// Buffer must be at least 20 characters long
// In case of error, "----Unknown time----" is written into buffer
// Returns 0 for success, -1 for failure
int get_date_time(char *buffer, size_t size);