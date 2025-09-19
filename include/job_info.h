#include <sys/types.h>

// Struct that stores information about a job
// It stores:
// - the four arguments that worker takes (source_directory, target_directory,filename, operation)
// - the process id of worker assigned to job (-1 if job hasn't been assigned to a worker yet)
// - a boolean variable sync_job that indicates if this job was requested by a sync command in the
//   console. This changes some messages that should be outputted.
struct job_info {
    char *src_dir;
    char *tar_dir;
    char *file;
    char *operation;
    pid_t worker_pid;
    int sync_job;
};