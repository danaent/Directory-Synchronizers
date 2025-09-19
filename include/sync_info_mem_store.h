#include <sys/types.h>

// Struct with info about monitored directory
struct sync_info_mem_store {
    char *src_dir;
    char *tar_dir;
    int wd;                  // File descriptor for inotify watch
    pid_t worker_pid;        // Pid of worker assigned for directory job, -1 if no worker is
                             // currently working on this directory
    char operation[9];       // Last operation performed (FULL, ADDED, MODIFIED, DELETED)
    int active;              // 1 if directory is active, 0 other wise. A directory is active
                             // if it is being monitored
    char last_sync_time[18];
    int error_count;
};