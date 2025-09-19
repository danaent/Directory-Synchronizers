#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <errno.h>
#include "../include/job_info.h"
#include "../include/int_queue.h"
#include "../include/worker_management.h"
#include <stdio.h>
#include <sys/inotify.h>

#define READ_END 0       // Read and write ends of pipe
#define WRITE_END 1
#define POLL_TIMEOUT -1  // Poll timeout (-1 means infinite)

#define CONSOLE_INDEX 0  // Index of fss_in pipe in pfds array
#define INOTIFY_INDEX 1  // Index of inotify instance in pfds array
                         // Rest of indexes is dedicated to worker pipes

int worker_manager_init(struct worker_manager *manager, int worker_limit, int console_fd) {

    // Active workers are initially 0
    manager->worker_limit = worker_limit;
    manager->active_workers = 0;

    // Allocate worker_jobs array
    // This array normally only requires worker_limit positions, but the first two
    // are unused to remain consistent with pfds array
    manager->worker_jobs = malloc((2+manager->worker_limit) * sizeof(struct job_info));
    if (manager->worker_jobs == NULL) return -1;

    // Allocate pfds array
    manager->pfds_size = 2 + worker_limit;
    manager->pfds = calloc(manager->pfds_size, sizeof(struct pollfd));

    if (manager->pfds == NULL) {
        free(manager->worker_jobs); return -1;
    }

    // Set up file descriptors for poll
    manager->pfds[CONSOLE_INDEX].fd = console_fd;
    manager->pfds[INOTIFY_INDEX].fd = inotify_init();

    if (manager->pfds[INOTIFY_INDEX].fd < 0) {
        free(manager->worker_jobs); free(manager->pfds);
        return -2;
    }

    manager->pfds[CONSOLE_INDEX].events = POLLIN;
    manager->pfds[INOTIFY_INDEX].events = POLLIN;
    
    // Initialize slot queue
    manager->slot_queue = int_queue_init();

    if (manager->slot_queue == NULL) {
        free(manager->worker_jobs); free(manager->pfds);
        close(manager->pfds[INOTIFY_INDEX].fd);
        return -1;
    }

    // Add all worker slots to queue
    for (size_t i = 2; i < manager->pfds_size; i++) {
        manager->pfds[i].fd = -1;
        manager->worker_jobs[i-2].worker_pid = -1;

        if (int_queue_enqueue(manager->slot_queue, i) < 0) {
            free(manager->worker_jobs); free(manager->pfds);
            close(manager->pfds[INOTIFY_INDEX].fd);
            int_queue_destroy(manager->slot_queue);
            return -1;
        }
    }

    return 0;
}

int worker_manager_available_workers(struct worker_manager manager) {
    return manager.worker_limit-manager.active_workers;
}

int worker_manager_active_workers(struct worker_manager manager) {
    return manager.active_workers;
}

int worker_manager_add_watch(struct worker_manager *manager, char *dir) {
    return inotify_add_watch(manager->pfds[INOTIFY_INDEX].fd, dir, IN_MODIFY | IN_CREATE | IN_DELETE);
}

int worker_manager_remove_watch(struct worker_manager *manager, int wd) {
    return inotify_rm_watch(manager->pfds[INOTIFY_INDEX].fd, wd);
}


pid_t worker_manager_setup_worker(struct worker_manager *manager, struct job_info job) {

    if (worker_manager_available_workers(*manager) == 0)
        return -1;

    // Get available worker slot
    int slot = int_queue_dequeue(manager->slot_queue);

    // Create pipe communication
    int pipefd[2];

    if (pipe(pipefd) < 0)
        return -2;

    // Save read end
    manager->pfds[slot].fd = pipefd[READ_END];
    manager->pfds[slot].events = POLLIN;

    // Fork
    pid_t pid = fork();

    if (pid < 0) {
        close(pipefd[READ_END]); close(pipefd[WRITE_END]);
        return -3;
    }

    // If this is the child
    if (pid == 0) {
        // Copy write end to stdout
        while (dup2(pipefd[WRITE_END], STDOUT_FILENO) < 0) {
            if (errno != EINTR) {
                close(pipefd[READ_END]); close(pipefd[WRITE_END]);
                return -4;
            }
        }

        // Close both previous ends
        close(pipefd[READ_END]); close(pipefd[WRITE_END]);

        // Call worker
        if (execl("./worker", "./worker", job.src_dir, job.tar_dir, job.file, job.operation, NULL) < 0) {
            return -5;
        }
    }

    // If this is the parent

    // Close write end
    close(pipefd[WRITE_END]);

    // Place job into array
    manager->worker_jobs[slot].src_dir = malloc((strlen(job.src_dir)+1) * sizeof(char));
    manager->worker_jobs[slot].tar_dir = malloc((strlen(job.tar_dir)+1) * sizeof(char));
    manager->worker_jobs[slot].file = malloc((strlen(job.file)+1) * sizeof(char));
    manager->worker_jobs[slot].operation = malloc((strlen(job.operation)+1) * sizeof(char));

    if (manager->worker_jobs[slot].src_dir == NULL || manager->worker_jobs[slot].tar_dir == NULL || manager->worker_jobs[slot].file == NULL || manager->worker_jobs[slot].operation == NULL) 
        return -1;

    strcpy(manager->worker_jobs[slot].src_dir, job.src_dir);
    strcpy(manager->worker_jobs[slot].tar_dir, job.tar_dir);
    strcpy(manager->worker_jobs[slot].file, job.file);
    strcpy(manager->worker_jobs[slot].operation, job.operation);
    manager->worker_jobs[slot].worker_pid = pid;
    manager->worker_jobs[slot].sync_job = job.sync_job;

    manager->active_workers++;
    return pid;
}

int worker_manager_free_worker(struct worker_manager *manager, int index) {
    // Close read end
    if (close(manager->pfds[index].fd) < 0)
        return -1;

    // Make worker available
    manager->pfds[index].fd = -1;
    int_queue_enqueue(manager->slot_queue, index);

    // Free resources
    free(manager->worker_jobs[index].file);
    free(manager->worker_jobs[index].src_dir);
    free(manager->worker_jobs[index].tar_dir);
    free(manager->worker_jobs[index].operation);
    manager->worker_jobs[index].worker_pid = -1;

    manager->active_workers--;

    return 0;
}

int worker_manager_index_is_console(struct worker_manager manager, int index) {
    return (index == CONSOLE_INDEX);
}

int worker_manager_index_is_inotify(struct worker_manager manager, int index) {
    return (index == INOTIFY_INDEX);
}

int worker_manager_index_is_worker(struct worker_manager manager, int index) {
    return (index >= 2 && index <= manager.worker_limit+1);
}

void worker_manager_destroy(struct worker_manager *manager) {
    int_queue_destroy(manager->slot_queue);
    free(manager->pfds);

    for (int i = 0; i < manager->worker_limit; i++) {
        if (manager->worker_jobs[i].worker_pid != -1) {
            free(manager->worker_jobs[i].file);
            free(manager->worker_jobs[i].src_dir);
            free(manager->worker_jobs[i].tar_dir);
            free(manager->worker_jobs[i].operation);
        }
    }
    free(manager->worker_jobs);
}
