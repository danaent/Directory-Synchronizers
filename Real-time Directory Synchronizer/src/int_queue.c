#include "../include/int_queue.h"

typedef struct node *Node;

struct node {
    int value;
    Node next;
};

struct int_queue {
    Node head;
    Node tail;
    size_t size;
};

IntQueue int_queue_init(void) {
    IntQueue queue = malloc(sizeof(struct int_queue));

    if (queue == NULL)
        return NULL;

    queue->head = queue->tail = NULL;
    queue->size = 0;
    return queue;
}

size_t int_queue_size(IntQueue queue) {
    return queue->size;
}

int int_queue_enqueue(IntQueue queue, int value) {

    // Allocate memory for node
    Node node = malloc(sizeof(struct node));
    if (node == NULL) return -1;

    node->value = value;
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

int int_queue_dequeue(IntQueue queue) {

    // If queue is empty
    if (queue->size == 0) {
        return 0;
    }

    // Remove head node
    Node old_head = queue->head;
    queue->head = queue->head->next;
    queue->size--;

    // Set tail to NULL if queue becomes empty
    if (queue->size == 0) queue->tail = NULL;

    // Return value
    int value = old_head->value;
    free(old_head);
    return value;
}

void int_queue_destroy(IntQueue queue) {

    Node node = queue->head;
    
    for (size_t i = 0; i < queue->size; i++) {
        Node next_node = node->next;
        free(node);
        node = next_node;
    }

    free(queue);
}