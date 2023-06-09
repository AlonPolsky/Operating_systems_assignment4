#include "queue.h"

Q* q;


/*======================= Auxilary functions =======================*/


void enqueue_node(Node** head, Node** tail, Node* inserted)
{
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

Node* pass_through_head(long ticket){
    Node* node  = q->head;
    while (node != NULL)
    {
        if (node->ticket == ticket)
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
    mtx_init(&(q->q_lock), mtx_plain | mtx_recursive);
    q->s = 0;
    q->cond_s = 0;
    q->visited = 0;
    q->ticket_generator = 1;
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

void test(char* s ,int* a)
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
        new_tail->ticket = ++(q->ticket_generator);
        q->cond_head->ticket = q->ticket_generator;
        cnd_signal(&(q->cond_head->cond));
        q->cond_head = q->cond_head->next;
        if(q->cond_head == NULL)
            q->cond_tail = NULL;
    }
    enqueue_node(&(q->head), &(q->tail), new_tail);
    q->s++;
    test("enqueued", (int*) enqueued);
    mtx_unlock(&(q->q_lock));
}


void* dequeue(void){
    mtx_lock(&(q->q_lock));
    void* returned;
    Node* node = pass_through_head(UNRESERVED);
    if (node == NULL)
    {
        fprintf(stderr, "going to sleep\n");
        test(NULL, NULL);
        Node* cond_node = (Node*) malloc(sizeof(Node));
        INIT_NODE(cond_node, NULL)
        cnd_init(&(cond_node->cond));
        q->cond_s++;
        enqueue_node(&(q->cond_head), &(q->cond_tail), cond_node);
        cnd_wait(&(cond_node->cond), &(q->q_lock));
        q->cond_s--;
        node = pass_through_head(cond_node->ticket);
        cnd_destroy(&(cond_node->cond));
        free(cond_node);
    }
    returned = node->content;
    if(node->next != NULL)
        node->next->prev = node->prev;
    if(node->prev != NULL)
        node->prev->next = node->next;
    if(node == q->head)
        q->head = node->next;
    if(node == q->tail)
        q->tail = node->prev;
    q->s--;
    q->visited++;
    free(node);
    test("dequeued", (int*) returned);
    mtx_unlock(&(q->q_lock));
    return returned;
}

bool tryDequeue(void** return_pointer){
    mtx_lock(&(q->q_lock));
    if(pass_through_head(UNRESERVED) == NULL)
    {
        mtx_unlock(&(q->q_lock));
        return false;
    }
    fprintf(stderr, "%lu successfuly tryDequeued.\n", thrd_current());
    *return_pointer = dequeue();
    mtx_unlock(&(q->q_lock));
    return true;
}

int func1(void* a)
{
    enqueue(a);
    return 0;
}

int func2(void*)
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