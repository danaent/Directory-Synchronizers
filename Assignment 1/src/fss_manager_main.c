#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/inotify.h>
#include "../include/util.h"
#include "../include/fss_manager.h"

#define WORKER_LIMIT_DEFAULT 5
#define BUF_SIZE 1024
#define DIR_NAME_SIZE 256
#define MIN_CONFIG_LINE_LENGTH 6
#define POLL_TIMEOUT -1

#define READ_END 0
#define WRITE_END 1

extern char *optarg;

char *fss_in = "fss_in";
char *fss_out = "fss_out";

void sigchld_handler(int signum);

int main(int argc, char *argv[]) {
    char *logfile_name = NULL;
    char *config_name = NULL;
    int worker_limit = -1;
   
    // Parse arguments
    int opt;
    while ((opt = getopt(argc, argv, "l:c:n:")) != -1) {
        switch(opt) {
            case 'l':
                logfile_name = optarg;
                break;
            case 'c':
                config_name = optarg;
                break;
            case 'n':
                worker_limit = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -l <manager_logfile> -c <config_file> [-n <worker_limit>]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (worker_limit <= 0) {
        worker_limit = WORKER_LIMIT_DEFAULT;
    }

    if (logfile_name == NULL || config_name == NULL) {
        fprintf(stderr, "Usage: %s -l <manager_logfile> -c <config_file> [-n <worker_limit>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Open log file for writiing
    int log_fd = open(logfile_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);

    if (log_fd < 0) { 
        perror("Failed to open logfile");
        exit(EXIT_FAILURE);
    }

    char datetime[20];
    char buffer[BUF_SIZE];

    // Open config file for reading
    FILE *config_file = fopen(config_name, "r");  

    if (config_file == NULL) {
        get_date_time(datetime, sizeof(datetime));
        snprintf(buffer, BUF_SIZE, "[%s] Failed to open %s: %s\n", datetime, config_name, strerror(errno));
        fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, NULL,  NULL, NULL, NULL, -1, -1, 1);
        exit(EXIT_FAILURE);
    }

    // Create named pipes for communication with console
    unlink(fss_in); unlink(fss_out);
    
    if (mkfifo(fss_in, 0644) < 0) {
        get_date_time(datetime, sizeof(datetime));
        snprintf(buffer, BUF_SIZE, "[%s] Named pipe \"%s\" failed: %s\n", datetime, fss_in, strerror(errno));
        fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  NULL, NULL, NULL, -1, -1, 1);
        exit(EXIT_FAILURE);
    }

    if (mkfifo(fss_out, 0644) < 0) {
        get_date_time(datetime, sizeof(datetime));
        snprintf(buffer, BUF_SIZE, "[%s] Named pipe \"%s\" failed: %s\n", datetime, fss_out, strerror(errno));
        fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  NULL, NULL, NULL, -1, -1, 1);
        unlink(fss_in);
        exit(EXIT_FAILURE);
    }

    int fss_in_fd = open(fss_in, O_RDWR);

    if (fss_in_fd < 0) {
        get_date_time(datetime, sizeof(datetime));
        snprintf(buffer, BUF_SIZE, "[%s] Open failed for named pipe \"%s\": %s\n", datetime, fss_in, strerror(errno));
        fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  NULL, NULL, NULL, -1, -1, 1);
        unlink(fss_in); unlink(fss_out);
        exit(EXIT_FAILURE);
    }

    int fss_out_fd = open(fss_out, O_RDWR);

    if (fss_out_fd < 0) {
        get_date_time(datetime, sizeof(datetime));
        snprintf(buffer, BUF_SIZE, "[%s] Open failed for named pipe \"%s\": %s\n", datetime, fss_out, strerror(errno));
        fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  NULL, NULL, NULL, fss_in_fd, -1, 1);
        unlink(fss_in); unlink(fss_out);
        exit(EXIT_FAILURE);
    }

    // Set signal handler
    struct sigaction act;
    act.sa_handler = sigchld_handler;
    sigfillset(&(act.sa_mask));
    act.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &act, NULL);

    // Initialize job queue
    JobQueue job_queue = job_queue_init();

    // Intialize file monitor
    FileMonitor file_monitor = file_monitor_init();

    if (job_queue == NULL || file_monitor == NULL) {
        get_date_time(datetime, sizeof(datetime));
        snprintf(buffer, BUF_SIZE, "[%s] Memory allocation failed\n", datetime);
        fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  file_monitor, job_queue, NULL, fss_in_fd, fss_out_fd, 1);
        unlink(fss_in); unlink(fss_out);
        exit(EXIT_FAILURE);
    }

    // Initialize worker manager
    struct worker_manager worker_manager;
    int err_check = worker_manager_init(&worker_manager, worker_limit, fss_in_fd);

    if (err_check < 0) {
        get_date_time(datetime, sizeof(datetime));

        if (err_check == -2)
            snprintf(buffer, BUF_SIZE, "[%s] inotify_init failed: %s\n", datetime, strerror(errno));
        else
            snprintf(buffer, BUF_SIZE, "[%s] Memory allocation failed\n", datetime);

        fss_abrupt_shutdown(buffer, BUF_SIZE, log_fd, config_file,  file_monitor, job_queue, NULL, fss_in_fd, fss_out_fd, 1);
        unlink(fss_in); unlink(fss_out);
        exit(EXIT_FAILURE);
    }

    // Get directory pairs from config file and start monitoring them
    if (fss_read_config_file(config_file, log_fd, job_queue, file_monitor, &worker_manager, fss_in_fd, fss_out_fd) < 0) {
        unlink(fss_in); unlink(fss_out);
        exit(EXIT_FAILURE);
    }

    // Run manager
    fss_manager_run(log_fd, config_file, file_monitor, job_queue, &worker_manager, fss_in_fd, fss_out_fd);
    
    unlink(fss_in);
    unlink(fss_out);
}

// Handler for SIGCHLD
void sigchld_handler(int signum) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}