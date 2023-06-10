
#include <stdbool.h>
#include <threads.h>
#include <stdlib.h>
#include <stdio.h>

#define INIT_NODE(node, cont) {\
    node->reserved = false;\
    node->content = cont;\
    node->next = NULL;\
    }

// The Nodes will have two functions, one is a Node that matches to a sleeping consumer, stored in the q->cond_head/q->cond_tail list,
// The second is a Node that stores an address added to the queue, stored in the q->head/q->tail list.
typedef struct Node{
    // cond will be used only if the Node matches to a sleeping consumer. It'll be used when a supplier fills the queue and
    // adds an address for the matching consumer by signalling awakenning time.
    cnd_t cond;
    struct Node* next;
    struct Node* prev; // prev will be maintained only if the Node matches to an address added to the list.
    // If the Node corresponds to a queue-item and reserved == true then ticket is the tid of the thread that supposed to dequeue content,
    // if the Node corresponds to a queue-item and reserved == false, ticket is undefined and if the Node corresponds to a sleeping thread then ticket is its tid.
    // ticket will be used as a way for a awakenning thread of finding the item it is supposed to dequeue.
    int ticket;
    // If the Node is corresponds to an queue-item, reserved == true iff the item was reserved for a blocked consumer, when the Node corresponds to a blocked consumer then reserved is undefined.
    bool reserved;
    void* content; // The address that matches the Node, won't be used in a case Node matches to a sleeeping thread.
}Node;

// Q represents the que.
typedef struct Q{
    mtx_t q_lock; // A lock for the q.
    size_t s; // The current amount of items in the queue.
    size_t cond_s; // An enumerator of the amount of threds currently sleeping.
    size_t visited; // An enumerator of the amount of items that have passed inside the queue
    Node* head; // head and tail attributes are for a list for item Nodes, reserved for items waiting for a consumer thread to take them.
    Node* tail;
    Node* cond_head; // cond_head and cond_tail are for a list condition variables a that match to sleeping threads.
    Node* cond_tail;
}Q;

Q* q;


/*======================= Auxilary functions =======================*/


// Adds inserted to the list represented by *head/*tail.
void enqueue_node(Node** head, Node** tail, Node* inserted)
{
    // inserted->next is unchanged because it was previously initiated in INIT_NODE to NULL.
    if(*tail == NULL)
    {
        *head = inserted;
        *tail = inserted;
        inserted->prev = NULL;
    }
    else
    {
        (*tail)->next = inserted;
        inserted->prev = *tail;
        *tail = inserted;
    }
}

// Destroys the the list that was given by head, cond == true iff the given list was the condition-variables list 
// because in that case destruction of the condition variables is needed.
void list_destroy(Node* head, bool cond)
{
    Node* temp;
    while (head != NULL)
    {
        temp = head;
        head = head->next;
        if(cond)
            cnd_destroy(temp->content);
        free(temp);
    }
}

// If the reserved parameter is true, tries to return a node in the q->head/q->tail list that has its ticket equal to the given ticket-parameter and its reserved equal to true,
// If the reserved paramet is false, tries to return a node in the list that has its reserved attribute equal to false. Anyway, if the requested node is inexistent, returns NULL.
Node* pass_through_head(bool reserved, long ticket){
    Node* node  = q->head;
    while (node != NULL)
    {
        if ((reserved == node->reserved) && ((!reserved) || (node->ticket == ticket)))
        {
            break;
        }
        node = node->next;
    }
    return node;
}

/*================================================================*/

void initQueue(void){
    q = (Q*) malloc(sizeof(Q));
    mtx_init(&(q->q_lock), mtx_plain | mtx_recursive); // recursive is for tryDequeue.
    q->s = 0;
    q->cond_s = 0;
    q->visited = 0;
    q->head = NULL;
    q->tail = NULL;
    q->cond_head = NULL;
    q->cond_tail = NULL;
}

void destroyQueue(void){
    mtx_destroy(&(q->q_lock));
    list_destroy(q->head, false);
    list_destroy(q->cond_head, true);
    free(q);
}

size_t size(void){
    return q->s;
}

size_t waiting(void)
{
    return q->cond_s;
}

size_t visited(void)
{
    return q->visited;
}

void test(char* s ,int* a) // remove.
{
    fprintf(stderr ,"tid: %lu \n", thrd_current());
    if(a != NULL)
        fprintf(stderr, "%s: %d \n", s, *a);
    fprintf(stderr ,"size: %lu \n", size());
    fprintf(stderr, "waiting: %lu \n", waiting());
    fprintf(stderr, "visited: %lu \n", visited());
    fprintf(stderr, "===============\n");
}


void enqueue(void* enqueued){
    mtx_lock(&(q->q_lock));
    Node* new_tail = (Node*) malloc(sizeof(Node));
    INIT_NODE(new_tail, enqueued)
    if(q->cond_head != NULL)
    {
        new_tail->reserved = true;
        new_tail->ticket = q->cond_head->ticket;
        cnd_signal(&(q->cond_head->cond));
        q->cond_head = q->cond_head->next;
        if(q->cond_head == NULL)
            q->cond_tail = NULL;
    }
    enqueue_node(&(q->head), &(q->tail), new_tail);
    q->s++;
    test("enqueued", (int*) enqueued); // remove.
    mtx_unlock(&(q->q_lock));
}


void* dequeue(void){
    mtx_lock(&(q->q_lock));
    void* returned;
    Node* node = pass_through_head(false, 0);
    if (node == NULL)
    {
        fprintf(stderr, "going to sleep\n"); // remove.
        Node* cond_node = (Node*) malloc(sizeof(Node));
        INIT_NODE(cond_node, NULL)
        cond_node->ticket = thrd_current();
        cnd_init(&(cond_node->cond));
        q->cond_s++;
        enqueue_node(&(q->cond_head), &(q->cond_tail), cond_node);
        test(NULL, NULL); // remove.
        cnd_wait(&(cond_node->cond), &(q->q_lock));
        q->cond_s--;
        node = pass_through_head(true, cond_node->ticket);
        cnd_destroy(&(cond_node->cond));
        free(cond_node);
    }
    returned = node->content;
    if(node->next != NULL)
        node->next->prev = node->prev;
    else
        q->tail = node->prev;
    if(node->prev != NULL)
        node->prev->next = node->next;
    else
        q->head = node->next;
    q->s--;
    q->visited++;
    free(node);
    test("dequeued", (int*) returned); // remove.
    mtx_unlock(&(q->q_lock));
    return returned;
}

bool tryDequeue(void** return_pointer){
    mtx_lock(&(q->q_lock));
    if(pass_through_head(false, 0) == NULL)
    {
        fprintf(stderr, "failed dequeue.\n"); // remove.
        test(NULL, NULL);
        mtx_unlock(&(q->q_lock));
        return false;
    }
    fprintf(stderr, "successfuly tryDequeued.\n"); // remove.
    *return_pointer = dequeue();
    mtx_unlock(&(q->q_lock));
    return true;
}

int func1(void* a)
{
    enqueue(a);
    return 0;
}

int func2(void* a)
{
    void* p;
    if(!tryDequeue(&p))
        dequeue();
    return 0;
}

int main(){
    for(int j = 0; j < 2; j++)
    {
        int a[5];
        thrd_t thrds[10];
        initQueue();
        for(int i = 0; i < 5; i++)
            a[i] = i;
        for(int i = 0; i < 10; i++)
        {
            if(i % 2)
                thrd_create(&thrds[i], func1, (&a[i/2]));
            else
                thrd_create(&thrds[i], func2, NULL);
        }
        for(int i = 0; i < 10; i++)
        {
            thrd_join(thrds[i], NULL);
        }
        destroyQueue();
    }
}