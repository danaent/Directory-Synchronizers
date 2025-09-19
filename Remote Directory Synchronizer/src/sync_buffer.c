#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include "../include/sync_buffer.h"

// A cyclical buffer of struct sync_pair
struct sync_buffer {
    struct sync_pair *data;
    int start;
    int end;
    int count;
    int size;
};

struct sync_buffer buffer;

pthread_mutex_t mutex;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;

int sync_buffer_quit_boolean = 0;


int sync_buffer_init(int size) {
    buffer.data = malloc(size *sizeof(struct sync_pair));
    if (buffer.data == NULL) return -1;

    buffer.start = 0;
    buffer.end = -1;
    buffer.count = 0;
    buffer.size = size;

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond_nonempty, NULL);
    pthread_cond_init(&cond_nonfull, NULL);

    return 0;
}

int sync_buffer_size(void) {
    return buffer.size;
}

int sync_buffer_count(void) {
    pthread_mutex_lock(&mutex);
    int count = buffer.count;
    pthread_mutex_unlock(&mutex);

    return count;
}

void sync_buffer_place(struct sync_pair pair) {
    
    pthread_mutex_lock(&mutex);

    // Wait on condtional variables if buffer is full
    while (buffer.count >= buffer.size) {
        pthread_cond_wait(&cond_nonfull, &mutex);
    }

    // Place pair
    buffer.end = (buffer.end + 1) % buffer.size;
    buffer.data[buffer.end] = pair;
    buffer.count++;

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond_nonempty);
}

void sync_buffer_obtain(struct sync_pair *pair) {

    pthread_mutex_lock(&mutex);

    // Wait on conditional variable if buffer is full 
    // and quit has not been called
    while (buffer.count <= 0 && !sync_buffer_quit_boolean) {
        pthread_cond_wait(&cond_nonempty, &mutex);
    }

    // Exit if quit has been called and there are no more items in the buffer
    if (buffer.count <= 0 && sync_buffer_quit_boolean) {
        pthread_mutex_unlock(&mutex);
        memset(pair, 0, sizeof(*pair));
        return;
    }

    // Remove pair and copy to pointer location
    *pair = buffer.data[buffer.start];
    buffer.start = (buffer.start + 1) % buffer.size;
    buffer.count--;

    pthread_mutex_unlock(&mutex);
    pthread_cond_signal(&cond_nonfull);
}

int sync_buffer_file_exists(struct sync_pair pair) {

    pthread_mutex_lock(&mutex);

    int exists = 0;

    // Go through buffer
    for (int i = 0; i < buffer.count; i++) {

        int search_index = (i + buffer.start) % buffer.size;
        struct sync_pair cur_pair = buffer.data[search_index];
        
        // If file is found
        if (!strcmp(pair.file, cur_pair.file) && !strcmp(pair.src_dir.dir, cur_pair.src_dir.dir) &&
            !strcmp(pair.src_dir.host, cur_pair.src_dir.host) && pair.src_dir.port == cur_pair.src_dir.port) {

            exists = 1;
            break;
        }
    }

    pthread_mutex_unlock(&mutex);
    return exists;
}

int sync_buffer_cancel_dir(char *src_dir_name, struct dir_location *deleted_buffer, int buffer_size) {
    int deleted = 0;

    pthread_mutex_lock(&mutex);

    // Index where next pair that will not be deleted will be placed
    int place_index = buffer.start;

    // Index of deleted buffer
    int deleted_index = 0;

    // Go through buffer
    for (int i = 0; i < buffer.count; i++) {

        // Index we will examine
        int search_index = (i + buffer.start) % buffer.size;

        // Directory at that index
        struct dir_location cur_dir = buffer.data[search_index].src_dir;

        // If that directory is different from src_dir
        if (strcmp(src_dir_name, cur_dir.dir)) {
            // Keep it by placing it in place_index
            // Move place_index to the right
            buffer.data[place_index] = buffer.data[search_index];
            place_index = (place_index + 1) % buffer.size;
        } else {
            deleted = 1;

            // Add directory to deleted buffer if it's not already in it
            int exists = 0;
            for (int j = 0; j < deleted_index; j++) {
                if (!strcmp(deleted_buffer[j].dir, cur_dir.dir) &&
                    !strcmp(deleted_buffer[j].host, cur_dir.host) &&
                    deleted_buffer[j].port == cur_dir.port) {
                    exists = 1;
                    break;
                }
            }

            if (!exists && deleted_index < buffer_size) {
                strcpy(deleted_buffer[deleted_index].dir, cur_dir.dir);
                strcpy(deleted_buffer[deleted_index].host, cur_dir.host);
                deleted_buffer[deleted_index].port = cur_dir.port;
                deleted_index++;
            }
        }
    }

    // Change buffer end and count
    buffer.end = (place_index - 1 + buffer.size) % buffer.size;
    buffer.count = (place_index - buffer.start + buffer.size) % buffer.size;

    pthread_mutex_unlock(&mutex);

    // If at least one pair has been deleted, buffer is not full
    if (deleted) {
        pthread_cond_signal(&cond_nonfull);
    }

    // Add NULL at end of deleted buffer
    memset(&deleted_buffer[deleted_index], 0, sizeof(struct dir_location));

    return deleted;
}

void sync_buffer_quit(void) {
    pthread_mutex_lock(&mutex);
    sync_buffer_quit_boolean = 1;
    pthread_mutex_unlock(&mutex);
    pthread_cond_broadcast(&cond_nonempty);
}

void sync_buffer_destroy(void) {
    free(buffer.data);
    pthread_cond_destroy(&cond_nonempty);
    pthread_cond_destroy(&cond_nonfull);
    pthread_mutex_destroy(&mutex);
}