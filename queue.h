#ifndef queue
#define queue

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <threads.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
void initQueue(void);
void destroyQueue(void);
void enqueue(void*);
void* dequeue(void);
bool tryDequeue(void**);
size_t size(void);
size_t waiting(void);
size_t visited(void);

#define UNRESERVED 0
#define INIT_NODE(node, cont) {\
    node->content = cont;\
    node->next = NULL;\
    node->ticket = UNRESERVED;\
    }


// This struct will have cond and prev set iff it matches to a sleeping thread. It'll use next and prev for dequing,
// In that case it'll be a node in the list referenced by head and tail in q, otherwise it'll be implemented it'll be a node in the list
// referenced by cond_head and cond_tail.
typedef struct Node{
    cnd_t cond;
    struct Node* next;
    struct Node* prev;
    long ticket;
    void* content;
}Node;

// Q represents the que. Head and tail attributes are for a list for item nodes, reserved for items waiting for a thread to take them.
// cond_head and cond_tail are lists of condition variables and items that match to sleeping threads.
typedef struct Q{
    mtx_t q_lock; // A lock for the q.
    size_t s; // s is for size.
    size_t cond_s; // An enumerator of the amount of threds currently sleeping.
    size_t visited; // An enumerator of the amount of threds that visited the q.
    long ticket_generator;
    Node* head; 
    Node* tail;
    Node* cond_head;
    Node* cond_tail;
}Q;

#endif