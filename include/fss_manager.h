#include "../include/file_monitor.h"
#include "../include/job_queue.h"
#include "../include/worker_management.h"

#define FSS_WRITE_LOG 1      // Writes to log file
#define FSS_WRITE_STDOUT 2   // Writes to stdout
#define FSS_WRITE_FSS_OUT 4  // Writes to fss_out pipe

// Writes contents of buffer to log file, stdout or fss_out depending on write_inst
// Write inst is a bitwise OR of the above marcros
// If write_inst indicates writing to fss_out and num_of_lines is not 0, this
// number if written to fss_out before buffer
// This way fss_console knows how many lines to read
void fss_log_event(char *buffer, int log_fd, int fss_out_fd, int num_of_lines, int write_inst);

// Reads directories from config_file and adds syncing jobs to queue
// Returns 0 for success, -1 if an error occurs
int fss_read_config_file(FILE *config_file, int log_fd, JobQueue job_queue, FileMonitor file_monitor, struct worker_manager *worker_manager, int fss_in_fd, int fss_out_fd);

// Main function that runs fss_manager
// Handles job queue, inotify events and console commands
void fss_manager_run(int log_fd, FILE *config_file, FileMonitor file_monitor, JobQueue job_queue, struct worker_manager *worker_manager, int fss_in_fd, int fss_out_fd);

// If an irrecoverable error occurs, such as malloc failure, this function prints out
// the message in buffer to the log file and stdout, prints out an abrupt shutdown message
// and clears all resources. It does not call exit, instead the program that called the
// function is responsible for exiting.
void fss_abrupt_shutdown(char *buffer, size_t buf_size, int log_fd, FILE *config_file, FileMonitor file_monitor, JobQueue job_queue, struct worker_manager *worker_manager, int fss_in_fd, int fss_out_fd, int num_of_lines);