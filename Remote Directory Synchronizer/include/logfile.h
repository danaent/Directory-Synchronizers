// An atomic log file structure
typedef struct logfile *LogFile;

// Initialize structure for file filename
int logfile_init(LogFile *log, char *filename);

// Atomically write buf_len bytes from buffer to log file
int logfile_write(LogFile log, char *buffer, int buf_len);

// Free resources for LogFile structure
void logfile_destroy(LogFile log);