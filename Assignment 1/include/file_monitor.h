#include "../include/sync_info_mem_store.h"



// This struct stores information about all added directories using struct sync_info_mem_store
typedef struct file_monitor *FileMonitor;

// Initializes file monitor, returns NULL if malloc fails
FileMonitor file_monitor_init(void);

// Returns number of files monitored
size_t file_monitor_size(FileMonitor monitor);

// Adds file src_dir to monitor, with target tar_dir and inotify watch descriptor wd
// Returns -1 if malloc fails, -2 if file is already monitored/active, and 0 otherwise
int file_monitor_add(FileMonitor monitor, char *src_dir, char *tar_dir, int wd);

// Returns 1 if there is a job done in this directory, 0 if not, and -1 if this directory
// is not in the monitor
int file_monitor_is_working(FileMonitor monitor, char *src_dir);

// Stops monitoring of src_dir
// Returns 0 on success, -1 if src_dir is not in monitor
int file_monitor_set_inactive(FileMonitor monitor, char *src_dir);

// Returns a pointer to struct sync_info_mem_store of src_dir
// If src_dir is set to NULL, returns pointer to struct sync_info_mem_store for directory
// with watch file desctiptor wd
// If requested entry is not in monitor, returns NULL
struct sync_info_mem_store *file_monitor_get_info(FileMonitor monitor, char *src_dir, int wd);

// Sets src_dir to working and changes necessary fields
// Returns 0 on success, -1 if src_dir is not in monitor
int file_monitor_set_working(FileMonitor monitor, char *src_dir, pid_t worker_pid, char *operation);

// Sets src_dir to not working and changes last_sync_time and error_count fields
int file_monitor_set_not_working(FileMonitor monitor, char *src_dir, char *time, int errors);

// Frees resources for file monitor
void file_monitor_destroy(FileMonitor monitor);