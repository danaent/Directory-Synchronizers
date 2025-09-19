#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/inotify.h>
#include <string.h>
#include "../include/fss_manager.h"
#include "../include/util.h"

#define BUF_SIZE 1024
#define DIR_NAME_SIZE 256
#define POLL_TIMEOUT -1

char buffer[BUF_SIZE];
char datetime[DATETIME_SZ];
char src_dir_name[DIR_NAME_SIZE];
char tar_dir_name[DIR_NAME_SIZE];

int fss_add_monitored_file(char *src_dir_name, char *tar_dir_name, int log_fd, FILE *config_file, FileMonitor file_monitor, JobQueue job_queue, struct worker_manager *worker_manager, int fss_in_fd, int fss_out_fd, int sync_job);
int fss_sync_file(char *src_dir_name, int log_fd, FILE *config_file, FileMonitor file_monitor, JobQueue job_queue, struct worker_manager *worker_manager, int fss_in_fd, int fss_out_fd);
int fss_read_worker_report(struct worker_manager *worker_manager, int i, char *buffer, size_t buf_size);

void fss_log_event(char *buffer, int log_fd, int fss_out_fd, int num_of_lines, int write_inst) {

    if (write_inst & FSS_WRITE_LOG)
        write_bytes(log_fd, buffer, strlen(buffer));

    if (write_inst & FSS_WRITE_STDOUT)
        write_bytes(STDOUT_FILENO, buffer, strlen(buffer));
    
    if (write_inst & FSS_WRITE_FSS_OUT) {
        char num_of_lines_str[100];
        snprintf(num_of_lines_str, 100, "%d\n", num_of_lines);  

        if (num_of_lines)
            write_bytes(fss_out_fd, num_of_lines_str, strlen(num_of_lines_str));

        write_bytes(fss_out_fd, buffer, strlen(buffer));
    }
}


int fss_read_config_file(FILE *config_file, int log_fd, JobQueue job_queue, FileMonitor file_monitor, struct worker_manager *worker_manager, int fss_in_fd, int fss_out_fd) {
    // Read line by line
    while (fgets(buffer, BUF_SIZE, config_file)) {

        // If line is parsed correctly
        if (sscanf(buffer, " (%255[^,], %255[^)])", src_dir_name, tar_dir_name) == 2) {
            // Check if file is being monitored already
            struct sync_info_mem_store *file_info = file_monitor_get_info(file_monitor, src_dir_name, 0);

            if (file_info != NULL) {
                get_date_time(datetime, sizeof(datetime));
                snprintf(buffer, BUF_SIZE, "[%s] Already in queue: %s\n", datetime, src_dir_name);
                fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT);

            } // If not, start monitoring
            else if (fss_add_monitored_file(src_dir_name, tar_dir_name, log_fd, config_file, file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd, 0) < 0)
                continue;

        // If line is not parsed correctly, shut down
        } else {
            // Log invalid format
            get_date_time(datetime, sizeof(datetime));
            snprintf(buffer, BUF_SIZE, "[%s] Invalid format in config file\n", datetime);
            fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file, file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd, 1);
            return -1;
        }
    }

    return 0;
}

void fss_manager_run(int log_fd, FILE *config_file, FileMonitor file_monitor, JobQueue job_queue, struct worker_manager *worker_manager, int fss_in_fd, int fss_out_fd) {

    int shut_down = 0; // Shutdown flag - set to 1 when command shutdown is read from console

    while (1) {
        // For every job in the queue
        size_t queue_size = job_queue_size(job_queue);

        for (size_t s = 0; s < queue_size; s++) {

            // Stop if there are no available workers
            if (worker_manager_available_workers(*worker_manager) == 0)
                break;

            // Take a job out of queue
            struct job_info job;
            if (job_queue_dequeue(job_queue, &job) < 0) {
                get_date_time(datetime, sizeof(datetime));
                snprintf(buffer, BUF_SIZE, "[%s] Memory allocation failed\n", datetime);
                return fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd, 1);
            }

            // If there is already a job performed for this directory, put job back to queue
            if (file_monitor_is_working(file_monitor, job.src_dir)) {
                if (job_queue_enqueue(job_queue, job.src_dir, job.tar_dir, job.file, job.operation, job.sync_job) < 0) {
                    get_date_time(datetime, sizeof(datetime));
                    snprintf(buffer, BUF_SIZE, "[%s] Memory allocation failed\n", datetime);
                    return fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd, 1);
                }

                free(job.src_dir); free(job.tar_dir); free(job.file); free(job.operation);
                continue;
            }

            // Set up worker with job
            pid_t worker_pid = worker_manager_setup_worker(worker_manager, job);

            // If worker is not set up, check error
            if (worker_pid < 0) {

                get_date_time(datetime, sizeof(datetime));

                if (worker_pid == -1) {
                    snprintf(buffer, BUF_SIZE, "[%s] Memory allocation failed\n", datetime);
                    free(job.src_dir); free(job.tar_dir); free(job.file); free(job.operation);
                    return fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd, 1);
                } else {
                    switch (worker_pid) {
                        case -2:
                            snprintf(buffer, BUF_SIZE, "[%s] [%s] [%s] [None] [%s] [ERROR] [File: %s - Pipe failed: %s]\n", datetime, job.src_dir, job.tar_dir, job.operation, job.file, strerror(errno));
                            break;
                        case -3:
                            snprintf(buffer, BUF_SIZE, "[%s] [%s] [%s] [None] [%s] [ERROR] [File: %s - Fork failed: %s]\n", datetime, job.src_dir, job.tar_dir, job.operation, job.file, strerror(errno));
                            break;
                        case -4:
                            snprintf(buffer, BUF_SIZE, "[%s] [%s] [%s] [None] [%s] [ERROR] [File: %s - Dup2 failed: %s]\n", datetime, job.src_dir, job.tar_dir, job.operation, job.file, strerror(errno));
                            break;
                        case -5:
                            snprintf(buffer, BUF_SIZE, "[%s] [%s] [%s] [None] [%s] [ERROR] [File: %s - Exec failed: %s]\n", datetime, job.src_dir, job.tar_dir, job.operation, job.file, strerror(errno));
                            break;
                        default:
                            snprintf(buffer, BUF_SIZE, "[%s] [%s] [%s] [None] [%s] [ERROR] [File: %s - Couldn't set up worker]\n", datetime, job.src_dir, job.tar_dir, job.operation, job.file);
                            break;
                    }

                    fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_LOG);

                    if (job.sync_job) {
                        snprintf(buffer, BUF_SIZE, "Sync failed %s -> %s\n", job.src_dir, job.tar_dir);
                        fss_log_event(buffer, log_fd, fss_out_fd, 0, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
                    }

                }
                
                free(job.src_dir); free(job.tar_dir); free(job.file); free(job.operation);
                continue;
            }

            // Set directory to working and update info
            file_monitor_set_working(file_monitor, job.src_dir, worker_pid, job.operation);

            // Free job resources
            free(job.src_dir); free(job.tar_dir);
            free(job.file); free(job.operation);
        }

        // If shutdown command has been received and there are no more jobs in the queue
        if (shut_down && job_queue_size(job_queue) == 0 && worker_manager_active_workers(*worker_manager) == 0) {
            worker_manager_destroy(worker_manager);
            file_monitor_destroy(file_monitor);
            job_queue_destroy(job_queue);

            close(fss_in_fd); fclose(config_file);

            get_date_time(datetime, sizeof(datetime));
            snprintf(buffer, BUF_SIZE, "[%s] Manager shutdown complete.\n", datetime);
            fss_log_event(buffer, log_fd, fss_out_fd, 0, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);

            close(log_fd); close(fss_out_fd);
            return;
        }

        // Poll
        while (poll(worker_manager->pfds, worker_manager->pfds_size, POLL_TIMEOUT) < 0) {
            if (errno != EINTR) {
                get_date_time(datetime, sizeof(datetime));
                snprintf(buffer, BUF_SIZE, "[%s] Poll failed: %s\n", datetime, strerror(errno));
                return fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd, 1);
            }
        }
        
        for (size_t i = 0; i < worker_manager->pfds_size; i++) {
            // Skip fds that aren't ready
            if (!(worker_manager->pfds[i].revents & POLLIN))
                continue;

            // If console is ready
            if (worker_manager_index_is_console(*worker_manager, i) && !shut_down) {
                // Read command from console
                read_line(fss_in_fd, buffer, BUF_SIZE);

                // Get command name
                char *tokenizer = " \n\t";
                char *com_name = strtok(buffer, tokenizer);

                // Command: add
                if (!strcmp(com_name, "add")) {
                    // Get source and target names
                    char *token = strtok(NULL, tokenizer);
                    strcpy(src_dir_name, token);

                    token = strtok(NULL, tokenizer);
                    strcpy(tar_dir_name, token);

                    // Add file
                    fss_add_monitored_file(src_dir_name, tar_dir_name, log_fd, config_file, file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd, 1);
        
                // Command: status
                } else if (!strcmp(com_name, "status")) {
                    char *token = strtok(NULL, tokenizer);
                    strcpy(src_dir_name, token);

                    get_date_time(datetime, sizeof(datetime));
                    struct sync_info_mem_store *file_info = file_monitor_get_info(file_monitor, src_dir_name, 0);

                    // If directory is not monitored
                    if (file_info == NULL) {
                        snprintf(buffer, BUF_SIZE, "[%s] Directory not monitored: %s\n", datetime, src_dir_name);
                        fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);

                    } else {
                        snprintf(buffer, BUF_SIZE, "[%s] Status requested for %s\n", datetime, src_dir_name);
                        fss_log_event(buffer, log_fd, fss_out_fd, 6, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);

                        snprintf(buffer, BUF_SIZE, "Directory: %s\nTarget: %s\nLast sync: %s\nErrors: %d\nStatus: %s\n", src_dir_name, file_info->tar_dir, file_info->last_sync_time, file_info->error_count, file_info->active? "Active": "Inactive");
                        write_bytes(STDOUT_FILENO, buffer, strlen(buffer));
                        write_bytes(fss_out_fd, buffer, strlen(buffer));
                    }

                // Command: cancel
                } else if (!strcmp(com_name, "cancel")) {
                    char *token = strtok(NULL, tokenizer);
                    strcpy(src_dir_name, token);

                    struct sync_info_mem_store *file_info = file_monitor_get_info(file_monitor, src_dir_name, 0);
                    get_date_time(datetime, sizeof(datetime));

                    if (file_info == NULL || !file_info->active) {
                        snprintf(buffer, BUF_SIZE, "[%s] Directory not monitored: %s\n", datetime, src_dir_name);
                        fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
                    } else {
                        if (worker_manager_remove_watch(worker_manager, file_info->wd) < 0) {
                            snprintf(buffer, BUF_SIZE, "[%s] Couldn't cancel %s - failed to remove inotify watch: %s\n", datetime, src_dir_name, strerror(errno));
                            fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
                        }
                        
                        file_monitor_set_inactive(file_monitor, src_dir_name);
                        job_queue_remove_dir(job_queue, src_dir_name);

                        snprintf(buffer, BUF_SIZE, "[%s] Monitoring stopped for %s\n", datetime, src_dir_name);
                        fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT | FSS_WRITE_LOG);
                    }

                // Command: sync
                } else if (!strcmp(com_name, "sync")) {
                    char *token = strtok(NULL, tokenizer);
                    strcpy(src_dir_name, token);

                    fss_sync_file(src_dir_name, log_fd, config_file, file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd);
                }
                else if (!strcmp(com_name, "shutdown")) {
                    snprintf(buffer, BUF_SIZE, "[%s] Shutting down manager...\n[%s] Waiting for all active workers to finish.\n[%s] Processing remaining queued tasks.\n", datetime, datetime, datetime);
                    fss_log_event(buffer, log_fd, fss_out_fd, 4, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
                    shut_down = 1;
                }

            // If inotify is ready
            } else if (worker_manager_index_is_inotify(*worker_manager, i) && !shut_down) {

                ssize_t bytes = read(worker_manager->pfds[i].fd, buffer, BUF_SIZE);

                // Read all events
                int j = 0;
                while (j < bytes) {
                    struct inotify_event *event;

                    event = (struct inotify_event *) &buffer[j];

                    // Find file with watch wd
                    struct sync_info_mem_store *file_info = file_monitor_get_info(file_monitor, NULL, event->wd);
                    int queue_check = 0;
                    
                    // Add job to queue
                    if (event->mask & IN_CREATE) {
                        queue_check = job_queue_enqueue(job_queue, file_info->src_dir, file_info->tar_dir, event->name, "ADDED", 0);
                    } else if (event->mask & IN_MODIFY) {
                        queue_check = job_queue_enqueue(job_queue, file_info->src_dir, file_info->tar_dir, event->name, "MODIFIED", 0);
                    } else if (event->mask & IN_DELETE) {
                        queue_check = job_queue_enqueue(job_queue, file_info->src_dir, file_info->tar_dir, event->name, "DELETED", 0);
                    } 

                    if (queue_check < 0) {
                        get_date_time(datetime, sizeof(datetime));
                        snprintf(buffer, BUF_SIZE, "[%s] Memory allocation failed\n", datetime);
                        return fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd, 1);
                    }

                    j += sizeof(struct inotify_event) + event->len;
                }


            }

            // If a worker is ready
            else if (worker_manager_index_is_worker(*worker_manager, i)) {
                int error_count = fss_read_worker_report(worker_manager, i, buffer, BUF_SIZE);

                fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_LOG);

                if (worker_manager->worker_jobs[i].sync_job) {
                    snprintf(buffer, BUF_SIZE, "Sync completed %s -> %s Errors: %d\n", worker_manager->worker_jobs[i].src_dir, worker_manager->worker_jobs[i].tar_dir, error_count);
                    fss_log_event(buffer, log_fd, fss_out_fd, 0, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
                }

                // Set directory to inactive
                file_monitor_set_not_working(file_monitor, worker_manager->worker_jobs[i].src_dir, datetime, error_count);

                // Free up worker
                worker_manager_free_worker(worker_manager, i);
            }
        }
    }
}

void fss_abrupt_shutdown(char *buffer, size_t buf_size, int log_fd, FILE *config_file, FileMonitor file_monitor, JobQueue job_queue, struct worker_manager *worker_manager, int fss_in_fd, int fss_out_fd, int num_of_lines) {
    fss_log_event(buffer, log_fd, fss_out_fd, num_of_lines+2, FSS_WRITE_STDOUT);

    get_date_time(datetime, DATETIME_SZ);

    snprintf(buffer, buf_size, "[%s] Shutting down abruptly.\n", datetime);
    fss_log_event(buffer, log_fd, fss_out_fd, 0, FSS_WRITE_STDOUT);

    if (worker_manager != NULL) worker_manager_destroy(worker_manager);
    if (file_monitor != NULL) file_monitor_destroy(file_monitor);
    if (job_queue != NULL) job_queue_destroy(job_queue);

    if (fss_in_fd >= 0) close(fss_in_fd);
    if (config_file != NULL) fclose(config_file);

    get_date_time(datetime, sizeof(datetime));
    snprintf(buffer, buf_size, "[%s] Manager shutdown complete.\n", datetime);
    fss_log_event(buffer, log_fd, fss_out_fd, 0, FSS_WRITE_STDOUT);

    close(log_fd); 
    if (fss_out_fd >= 0) close(fss_out_fd);
}

// Begins monitoring of a file, returns 0 for success, -1 for failure
int fss_add_monitored_file(char *src_dir_name, char *tar_dir_name, int log_fd, FILE *config_file, FileMonitor file_monitor, JobQueue job_queue, struct worker_manager *worker_manager, int fss_in_fd, int fss_out_fd, int sync_job) {

    // Check if file is being monitored already
    struct sync_info_mem_store *file_info = file_monitor_get_info(file_monitor, src_dir_name, 0);

    if (file_info != NULL && file_info->active) {
        get_date_time(datetime, sizeof(datetime));
        snprintf(buffer, BUF_SIZE, "[%s] Already in queue: %s\n", datetime, src_dir_name);

        if (sync_job)
            fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
        else
            fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT);
        return 0;
    }
    
    // Add file watch to worker manager
    int wd = worker_manager_add_watch(worker_manager, src_dir_name);

    if (wd < 0) {
        get_date_time(datetime, sizeof(datetime));
        snprintf(buffer, BUF_SIZE, "[%s] Unable to start monitoring %s -> %s: %s\n", datetime, src_dir_name, tar_dir_name, strerror(errno));
        
        if (sync_job)
            fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
        else
            fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT);
        return -1;
    }

    // Add to file monitor
    if (file_monitor_add(file_monitor, src_dir_name, tar_dir_name, wd) < 0) {
        get_date_time(datetime, sizeof(datetime));
        snprintf(buffer, BUF_SIZE, "[%s] Memory allocation failed\n", datetime);
        fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd, 1);
        return -1;
    }
    

    // Add job to queue
    if (job_queue_enqueue(job_queue, src_dir_name, tar_dir_name, "ALL", "FULL", 0) < 0) {
        get_date_time(datetime, sizeof(datetime));
        snprintf(buffer, BUF_SIZE, "[%s] Memory allocation failed\n", datetime);
        fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  file_monitor, job_queue, worker_manager, fss_in_fd, fss_out_fd, 1);
        return -1;
    }

    // Write to log file
    get_date_time(datetime, sizeof(datetime));
    snprintf(buffer, BUF_SIZE, "[%s] Added directory: %s -> %s\n[%s] Monitoring started for %s\n", datetime, src_dir_name, tar_dir_name, datetime, src_dir_name);
    
    if (sync_job)
        fss_log_event(buffer, log_fd, fss_out_fd, 2, FSS_WRITE_LOG | FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
    else
        fss_log_event(buffer, log_fd, fss_out_fd, 2, FSS_WRITE_LOG | FSS_WRITE_STDOUT);

    return 0;
}

// Begins a full sync job for src_dir_name
int fss_sync_file(char *src_dir_name, int log_fd, FILE *config_file, FileMonitor file_monitor, JobQueue job_queue, struct worker_manager *worker_manager, int fss_in_fd, int fss_out_fd) {
    
    // Get file info
    struct sync_info_mem_store *file_info = file_monitor_get_info(file_monitor, src_dir_name, 0);
    get_date_time(datetime, sizeof(datetime));

    // If file does not exist
    if (file_info == NULL) {
        snprintf(buffer, BUF_SIZE, "[%s] Directory not monitored: %s\n", datetime, src_dir_name);
        fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);

    // If there is already a job performed or queued for this directory
    } else if (file_info->worker_pid != -1 || job_queue_dir_exists(job_queue, src_dir_name)) {
        snprintf(buffer, BUF_SIZE, "[%s] Sync already in progress %s\n", datetime, src_dir_name);
        fss_log_event(buffer, log_fd, fss_out_fd, 1, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);

    // Begin sync
    } else {
        strcpy(tar_dir_name, file_info->tar_dir);
        snprintf(buffer, BUF_SIZE, "[%s] Syncing directory: %s -> %s\n", datetime, src_dir_name, tar_dir_name);
        fss_log_event(buffer, log_fd, fss_out_fd, 2, FSS_WRITE_LOG | FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);

        // If file is not active
        if (!file_info->active) {
            // Add file watch to worker manager
            int wd = worker_manager_add_watch(worker_manager, src_dir_name);

            if (wd < 0) {
                get_date_time(datetime, sizeof(datetime));
                snprintf(buffer, BUF_SIZE, "[%s] Unable to start monitoring %s -> %s: %s\n", datetime, src_dir_name, tar_dir_name, strerror(errno));
                fss_log_event(buffer, log_fd, fss_out_fd, 0, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
                return -1;
            }

            // Add to file monitor
            if (file_monitor_add(file_monitor, src_dir_name, tar_dir_name, wd) < 0) {
                get_date_time(datetime, sizeof(datetime));
                snprintf(buffer, BUF_SIZE, "[%s] Unable to start monitoring %s -> %s: %s\n", datetime, src_dir_name, tar_dir_name, strerror(errno));
                fss_log_event(buffer, log_fd, fss_out_fd, 0, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
                return -1;
            }
        }

        // Add job to queue
        if (job_queue_enqueue(job_queue, src_dir_name, tar_dir_name, "ALL", "FULL", 1) < 0) {
            get_date_time(datetime, sizeof(datetime));
            snprintf(buffer, BUF_SIZE, "[%s] Unable to start monitoring %s -> %s: %s\n", datetime, src_dir_name, tar_dir_name, strerror(errno));
            fss_log_event(buffer, log_fd, fss_out_fd, 0, FSS_WRITE_STDOUT | FSS_WRITE_FSS_OUT);
            return -1;
        }
    }

    return 0;
}

// Reads and parses report from worker at index i of worker manager and writes logging message to buffer of buf_size
int fss_read_worker_report(struct worker_manager *worker_manager, int i, char *buffer, size_t buf_size) {
    int report_ok = 1;  // Set to 0 if report does not follow format
    char status[8];
    char details[100];
    char error[100];

    // Get report of worker
    read_line(worker_manager->pfds[i].fd, buffer, BUF_SIZE);
    if (strcmp(buffer, "EXEC_REPORT_START\n"))
        report_ok = 0;

    // Get status
    if (report_ok) {
        read_line(worker_manager->pfds[i].fd, buffer, BUF_SIZE);

        if (sscanf(buffer, "STATUS: %[^\n]", status) != 1)
            report_ok = 0;
    } else {
        strcpy(status, "Unknown");
    }

    // Get details
    if (report_ok) {
        read_line(worker_manager->pfds[i].fd, buffer, BUF_SIZE);

        if (sscanf(buffer, "DETAILS: %[^\n]", details) != 1) 
            report_ok = 0;
    } else {
        strcpy(details, "Unknown");
    }

    // Get first error if it exists and count errors
    read_line(worker_manager->pfds[i].fd, buffer, BUF_SIZE);

    int error_count = 0;

    if (!strcmp(buffer, "ERRORS:\n")) {
        read_line(worker_manager->pfds[i].fd, error, 100);
        error_count++;

        read_line(worker_manager->pfds[i].fd, buffer, BUF_SIZE);

        while(strcmp(buffer, "EXEC_REPORT_END\n")) {
            error_count++;
            read_line(worker_manager->pfds[i].fd, buffer, BUF_SIZE);
        }
    }

    // Get date and time
    get_date_time(datetime, sizeof(datetime));

    // Write to buffer
    if (!strcmp(worker_manager->worker_jobs[i].operation, "FULL")) {
        snprintf(buffer, BUF_SIZE, "[%s] [%s] [%s] [%d] [%s] [%s] [%s]\n", 
        datetime, worker_manager->worker_jobs[i].src_dir, worker_manager->worker_jobs[i].tar_dir, worker_manager->worker_jobs[i].worker_pid, worker_manager->worker_jobs[i].operation, status, details);
    } else if (!strcmp(status, "SUCCESS")) {
        snprintf(buffer, BUF_SIZE, "[%s] [%s] [%s] [%d] [%s] [%s] [File: %s]\n", 
        datetime, worker_manager->worker_jobs[i].src_dir, worker_manager->worker_jobs[i].tar_dir, worker_manager->worker_jobs[i].worker_pid, worker_manager->worker_jobs[i].operation, status, worker_manager->worker_jobs[i].file);
    } else {
        snprintf(buffer, BUF_SIZE, "[%s] [%s] [%s] [%d] [%s] [%s] [%s]\n", 
        datetime, worker_manager->worker_jobs[i].src_dir, worker_manager->worker_jobs[i].tar_dir, worker_manager->worker_jobs[i].worker_pid, worker_manager->worker_jobs[i].operation, status, error+1);    
    }

    return error_count;
}

