#include <string.h>
#include "../include/job_queue.h"

typedef struct node *Node;

struct node {
    struct job_info job;
    Node next;
};

struct job_queue {
    Node head;
    Node tail;
    size_t size;
};

JobQueue job_queue_init(void) {
    JobQueue queue = malloc(sizeof(struct job_queue));

    if (queue == NULL)
        return NULL;

    queue->head = queue->tail = NULL;
    queue->size = 0;
    return queue;
}

size_t job_queue_size(JobQueue queue) {
    return queue->size;
}

int job_queue_enqueue(JobQueue queue, char *src_dir, char *tar_dir, char *file, char *operation, int sync_job) {

    // Allocate memory for node
    Node node = malloc(sizeof(struct node));
    if (node == NULL) return -1;

    node->job.src_dir = malloc((strlen(src_dir)+1) * sizeof(char));

    if (node->job.src_dir == NULL) {
        free(node); return -1;
    }

    node->job.tar_dir = malloc((strlen(tar_dir)+1) * sizeof(char));

    if (node->job.tar_dir == NULL) {
        free(node->job.src_dir); free(node);
        return -1;
    }

    
    node->job.file = malloc((strlen(file)+1) * sizeof(char));

    if (node->job.file == NULL) {
        free(node->job.src_dir); free(node->job.tar_dir);
        free(node);
        return -1;
    }

    node->job.operation = malloc((strlen(operation)+1) * sizeof(char));

    if (node->job.operation == NULL) {
        free(node->job.src_dir); free(node->job.tar_dir); free(node->job.file);
        free(node); 
        return -1;
    }

    strcpy(node->job.src_dir, src_dir);
    strcpy(node->job.tar_dir, tar_dir);
    strcpy(node->job.file, file);
    strcpy(node->job.operation, operation);

    node->job.worker_pid = -1;
    node->job.sync_job = sync_job;
    node->next = NULL;

    // Add node to queue
    if (queue->size == 0) {
        queue->head = queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = queue->tail->next;
    }

    queue->size++;
    return 0;
}

int job_queue_dequeue(JobQueue queue, struct job_info *job) {
    
    if (queue->size == 0) {
        job->file = NULL;
        job->src_dir = NULL;
        job->tar_dir = NULL;
        job->operation = NULL;
        job->worker_pid = 0;
        job->sync_job = 0;
        return 0;
    }

    Node old_head = queue->head;
    queue->head = queue->head->next;
    queue->size--;

    // Allocate memory for job
    if (queue->size == 0) queue->tail = NULL;

    job->file = malloc((strlen(old_head->job.file)+1) * sizeof(char));

    if (job->file == NULL) {
        return -1;
    }

    job->src_dir = malloc((strlen(old_head->job.src_dir)+1) * sizeof(char));

    if (job->src_dir == NULL) {
        free(job->file); return -1;
    }

    job->tar_dir = malloc((strlen(old_head->job.tar_dir)+1) * sizeof(char));

    if (job->tar_dir == NULL) {
        free(job->file); free(job->src_dir);
        return -1;
    }

    job->operation = malloc((strlen(old_head->job.operation)+1) * sizeof(char));

    if (job->operation == NULL) {
        free(job->file); free(job->src_dir); free(job->tar_dir);
        return -1;
    }

    // Copy job info
    strcpy(job->file, old_head->job.file);
    strcpy(job->src_dir, old_head->job.src_dir);
    strcpy(job->tar_dir, old_head->job.tar_dir);
    strcpy(job->operation, old_head->job.operation);
    job->worker_pid = old_head->job.worker_pid;
    job->sync_job = old_head->job.sync_job;

    // Free old memory
    free(old_head->job.file); free(old_head->job.src_dir); free(old_head->job.tar_dir); free(old_head->job.operation);
    free(old_head);
    return 0;
}

int job_queue_dir_exists(JobQueue queue, char *dir) {
    
    Node cur_node = queue->head;

    while (cur_node != NULL) {
        if (!strcmp(dir, cur_node->job.src_dir))
            return 1;

        cur_node = cur_node->next;
    }

    return 0;
}

void job_queue_remove_dir(JobQueue queue, char *dir) {
    if (queue->size == 0)
        return;

    Node cur_node = queue->head;

    while (!strcmp(queue->head->job.src_dir, dir)) {
        queue->head = queue->head->next;

        free(cur_node->job.file); free(cur_node->job.src_dir); free(cur_node->job.tar_dir); free(cur_node->job.operation);
        free(cur_node);

        queue->size--;

        if (queue->size == 0) {
            queue->tail = NULL;
            return;
        }

        cur_node = queue->head;
    }

    Node prev_node = cur_node;
    cur_node = cur_node->next;

    while (cur_node != NULL) {
        if (!strcmp(cur_node->job.src_dir, dir)) {
            prev_node->next = cur_node->next;

            free(cur_node->job.file); free(cur_node->job.src_dir); free(cur_node->job.tar_dir); free(cur_node->job.operation);
            free(cur_node);

            queue->size--;
            cur_node = prev_node->next;

            if (cur_node == NULL) {
                queue->tail = NULL;
            }

        } else {
            cur_node = cur_node->next;
        }
    }
}

void job_queue_destroy(JobQueue queue) {

    Node node = queue->head;
    
    for (size_t i = 0; i < queue->size; i++) {
        Node next_node = node->next;

        free(node->job.file); free(node->job.src_dir); free(node->job.tar_dir); free(node->job.operation);
        free(node);

        node = next_node;
    }

    free(queue);
}