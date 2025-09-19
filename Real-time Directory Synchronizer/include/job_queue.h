#include <stdlib.h>
#include "../include/job_info.h"

// Queue of jobs that haven't been completed yet
typedef struct job_queue *JobQueue;

// Initializes job queue, returns NULL if malloc fails
JobQueue job_queue_init(void);

// Returns job queue size
size_t job_queue_size(JobQueue queue);

// Creates a job with given fields and adds it to the queue
// Returns -1 if malloc fails, 0 otherwise
int job_queue_enqueue(JobQueue queue, char *src_dir, char *tar_dir, char *file, char *operation, int sync_job);

// Removes a job from queue and copies its fields to job. Also allocates memory for fields in job.
// Returns -1 if malloc fails, 0 otherwise
// If queue is empty it sets all fields of job to NULL, then returns 0
int job_queue_dequeue(JobQueue queue, struct job_info *job);

// Returns 1 if there is a job for dir in queue, otherwise returns 0
int job_queue_dir_exists(JobQueue queue, char *dir);

// Removes all jobs for dir from queue
void job_queue_remove_dir(JobQueue queue, char *dir);

// Frees resources for queue
void job_queue_destroy(JobQueue queue);