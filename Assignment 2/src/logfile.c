#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "../include/logfile.h"
#include "../include/util.h"

struct logfile {
    int fd;                // File descriptor
    pthread_mutex_t mutex; // Mutex for atomic write
};

int logfile_init(LogFile *log, char *filename) {
    // Allocate memory for struct
    *log = malloc(sizeof(struct logfile));
    if (*log == NULL) return -1;

    // Open logfile
    (*log)->fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if ((*log)->fd < 0) return -1;

    // Initialize mutex
    pthread_mutex_init(&((*log)->mutex), NULL);

    return 0;
}

int logfile_write(LogFile log, char *buffer, int buf_len) {
    pthread_mutex_lock(&(log->mutex));

    int err = write_bytes(log->fd, buffer, buf_len);

    pthread_mutex_unlock(&(log->mutex));

    if (err < 0) return -1;
    else return 0;
}

void logfile_destroy(LogFile log) {
    close(log->fd);
    pthread_mutex_destroy(&(log->mutex));
    free(log);
}