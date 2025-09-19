#include <stdlib.h>

// A FIFO queue for int data type
typedef struct int_queue *IntQueue;

// Initializes int queue, returns NULL if malloc fails
IntQueue int_queue_init(void);

// Returns int queue size
size_t int_queue_size(IntQueue queue);

// Adds value to queue, returns -1 if malloc fails, 0 otherwise
int int_queue_enqueue(IntQueue queue, int value);

// Removes a value from queue and returns it
// Returns 0 if queue is empty
int int_queue_dequeue(IntQueue queue);

// Frees up resources for queue
void int_queue_destroy(IntQueue queue);