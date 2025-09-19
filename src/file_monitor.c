#include <string.h>
#include <stdlib.h>
#include "../include/file_monitor.h"

typedef struct node *Node;

struct node {
    struct sync_info_mem_store info;
    Node next;
};

struct file_monitor { // File monitor is a linked list
    Node head;
    Node tail;
    size_t size;
};

FileMonitor file_monitor_init(void) {
    FileMonitor monitor = malloc(sizeof(struct file_monitor));

    if (monitor == NULL)
        return NULL;

    monitor->head = monitor->tail = NULL;
    monitor->size = 0;
    return monitor;
}

size_t file_monitor_size(FileMonitor monitor) {
    return monitor->size;
}

int file_monitor_add(FileMonitor monitor, char *src_dir, char *tar_dir, int wd) {

    struct sync_info_mem_store *info = file_monitor_get_info(monitor, src_dir, 0);

    // If this file is already in monitor
    if (info != NULL) {
        // If it's active, no need to add anything
        if (info->active)
            return -2;

        // If it's inactive, start monitoring
        info->active = 1;
        info->wd = wd;

        // Update target directory
        free(info->tar_dir);

        info->tar_dir = malloc((strlen(tar_dir) + 1) * sizeof(char));
        strcpy(info->tar_dir, tar_dir);

        return 0;
    } 

    // If file is not in monitor

    // Allocate memory for node
    Node node = malloc(sizeof(struct node));
    if (node == NULL) return -1;

    // Allocate memory for src_dir and tar_dir
    node->info.src_dir = malloc((strlen(src_dir) + 1) * sizeof(char));

    if (node->info.src_dir == NULL) {
        free(node); return -1;
    }

    node->info.tar_dir = malloc((strlen(tar_dir) + 1) * sizeof(char));

    if (node->info.tar_dir == NULL) {
        free(node); free(node->info.src_dir);
        return -1;
    }

    // Add info
    strcpy(node->info.src_dir, src_dir);
    strcpy(node->info.tar_dir, tar_dir);

    node->info.wd = wd;
    node->info.worker_pid = -1;
    node->info.active = 1;
    node->info.last_sync_time[0] = '\0';
    node->info.error_count = 0;
    
    node->next = NULL;

    // Add node to list
    if (monitor->size == 0) {
        monitor->head = monitor->tail = node;
    } else {
        monitor->tail->next = node;
        monitor->tail = monitor->tail->next;
    }

    monitor->size++;
    return 0;
}

int file_monitor_is_working(FileMonitor monitor, char *src_dir) {
    if (monitor->size == 0)
        return -1;

    // Iterate over list
    Node cur_node = monitor->head;

    while (cur_node != NULL) {
        if (!strcmp(cur_node->info.src_dir, src_dir)) {
            // Check if there's a job in this directory
            if (cur_node->info.worker_pid != -1) {
                return 1;
            } else {
                return 0;
            }
        }

        cur_node = cur_node->next;
    }

    return -1;
}

int file_monitor_set_inactive(FileMonitor monitor, char *src_dir) {
    
    struct sync_info_mem_store *file_info = file_monitor_get_info(monitor, src_dir, 0);

    if (file_info == NULL) 
        return -1;

    file_info->active = 0;
    return 0;
}

struct sync_info_mem_store *file_monitor_get_info(FileMonitor monitor, char *src_dir, int wd) {
    if (monitor->size == 0)
        return NULL;

    Node cur_node = monitor->head;

    // Search for wd
    if (src_dir == NULL) {
        while (cur_node != NULL) {
            if (cur_node->info.wd == wd) {
                return &cur_node->info;
            }
    
            cur_node = cur_node->next;
        }
    // Search for src_dir
    } else {
        while (cur_node != NULL) {
            if (!strcmp(cur_node->info.src_dir, src_dir)) {
                return &cur_node->info;
            }
    
            cur_node = cur_node->next;
        }
    }

    return NULL;
}

int file_monitor_set_working(FileMonitor monitor, char *src_dir, pid_t worker_pid, char *operation) {
    struct sync_info_mem_store *info = file_monitor_get_info(monitor, src_dir, 0);
    if (info == NULL) return -1;

    info->worker_pid = worker_pid;
    strcpy(info->operation, operation);

    return 0;
}

int file_monitor_set_not_working(FileMonitor monitor, char *src_dir, char *time, int errors) {
    struct sync_info_mem_store *info = file_monitor_get_info(monitor, src_dir, 0);
    if (info == NULL) return -1;

    info->worker_pid = -1;
    strcpy(info->last_sync_time, time);
    info->error_count += errors;

    return 0;
}

void file_monitor_destroy(FileMonitor monitor) {
    Node cur_node = monitor->head;

    while (cur_node != NULL) {
        Node next_node = cur_node->next;

        free(cur_node->info.src_dir); free(cur_node->info.tar_dir);
        free(cur_node);

        cur_node = next_node;
    }

    free(monitor);
}