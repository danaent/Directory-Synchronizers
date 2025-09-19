#include <sys/types.h>
#include "../include/int_queue.h"

// This struct is responsible for:
// - Storing information for currently active workers
// - Setting up workers for jobs
// - Storing necessary open files for polling, i.e. console input, inotify instance and worker pipes
struct worker_manager {
    int worker_limit;             // Maximum number of active workers
    int active_workers;           // Number of currently active workers - this is the same as slot queue size
    struct job_info *worker_jobs; // Worker and job information, such as command line arguments and pid
    size_t pfds_size;             // Size of pfds array
    struct pollfd *pfds;          // Array of file descriptors to keep track on
    IntQueue slot_queue;          // Queue of next available worker slot
};

// Initializes manager
// Returns -1 if malloc fails and -2 if inotify_init fails
int worker_manager_init(struct worker_manager *manager, int worker_limit, int console_fd);

// Returns the number of available workers
int worker_manager_available_workers(struct worker_manager manager);

int worker_manager_active_workers(struct worker_manager manager);

// Adds an inotify watch for dir, returns file descriptor or -1 in case of error
int worker_manager_add_watch(struct worker_manager *manager, char *dir);

// Removes inotify watch wd, returns 0 on success, -1 on error
int worker_manager_remove_watch(struct worker_manager *manager, int wd);

// Assigns a worker to job from struct job
// Sets up pipe communication and executes worker child
// On success, returns pid of worker child. On error, returns one of the following:
// ERROR CODES:
// -1: malloc failed
// -2: pipe failed
// -3: fork failed
// -4: dup2 failed
// -5: exec failed
pid_t worker_manager_setup_worker(struct worker_manager *manager, struct job_info job);

// Makes worker slot at index available after job is done, frees up resources and
// closes pipe communication 
int worker_manager_free_worker(struct worker_manager *manager, int index);

// Returns 1 if index is console file descriptor's index in pfds array, otherwise
// returns 0
int worker_manager_index_is_console(struct worker_manager manager, int index);

// Returns 1 if index is inotify instance file descriptor index in pfds array,
// otherwise returns 0
int worker_manager_index_is_inotify(struct worker_manager manager, int index);

// Returns 1 if index is a worker pipe file descriptor index in pfds array,
// otherwise returns 0
int worker_manager_index_is_worker(struct worker_manager manager, int index);

// Frees up resources for manager
void worker_manager_destroy(struct worker_manager *manager);