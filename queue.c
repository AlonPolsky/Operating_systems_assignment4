#include "queue.h"

Q* q;


/*======================= Auxilary functions =======================*/


void enqueue_node(Node** head, Node** tail, Node* inserted)
{
    if(*tail == NULL)
    {
        *head = inserted;
        *tail = inserted;
    }
    else
    {
        (*tail)->next = inserted;
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


/*================================================================*/

void initQueue(void){
    q = (Q*) malloc(sizeof(Q));
    mtx_init(&(q->q_lock), mtx_recursive);
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

void test(char* s ,int* a)
{
    fprintf(stderr ,"tid: %lu \n", thrd_current());
    if(a != NULL)
        printf("%s: %d \n", s, *a);
    fprintf(stderr ,"size: %lu \n", size());
    fprintf(stderr, "waiting: %lu \n", waiting());
    fprintf(stderr, "visited: %lu \n", visited());
}


void enqueue(void* enqued){
    mtx_lock(&(q->q_lock));
    if(q->cond_head == NULL)
    {
        Node* new_tail = (Node*) malloc(sizeof(Node));
        //printf("allocing: %p\n", &(*new_tail));
        new_tail->content = enqued;
        enqueue_node(&(q->head), &(q->tail), new_tail);
    }
    else
    {
        q->cond_head->content = enqued;
        cnd_signal(&(q->cond_head->cond));
        q->cond_head = q->cond_head->next;
        if(q->cond_head == NULL)
            q->cond_tail = NULL;
    }
    q->s++;
    //test("enque", enqued);
    mtx_unlock(&(q->q_lock));
}


void* dequeue(void){
    mtx_lock(&(q->q_lock));
    void* returned;
    Node* node;
    if (q->head == NULL)
    {
        q->cond_s++;
        node = (Node*) malloc(sizeof(Node));
        node->next = NULL;
        cnd_init(&(node->cond));
        enqueue_node(&(q->cond_head), &(q->cond_tail), node);
        cnd_wait(&(node->cond), &(q->q_lock));
        q->cond_s--;
        cnd_destroy(&(node->cond));
        returned = node->content;
        free(node);
    }
    else
    {
        node = q->head;
        q->head = q->head->next;
        if (q->head == NULL)
        {
            q->tail = NULL;
        }
        returned = node->content;
        // printf("freeing: %p\n", &(*node));
        free(node);
    }
    q->s--;
    q->visited++;
    // test("dequeue", returned);
    mtx_unlock(&(q->q_lock));
    return returned;
}

bool tryDequeue(void** return_pointer){
    mtx_lock(&(q->q_lock));
    if(q->head == NULL)
    {
        mtx_unlock(&(q->q_lock));
        return false;
    }
    *return_pointer = dequeue();
    // fprintf(stderr, "%lu successfuly terDequeued." ,thrd_current());
    mtx_unlock(&(q->q_lock));
    return true;
}

int func(void* a)
{
    void* p;
    enqueue(a);
    dequeue();
    // else
    //     printf("try dequeue result: %d:, tid: %lu \n", *((int*)p), thrd_current());
    return 0;
}

int main(){
    for(int j = 0; j < 2; j++)
    {
        int a[20];
        thrd_t thrds[20];
        initQueue();
        for(int i = 0; i < 20; i++)
            a[i] = i;
        for(int i = 0; i < 20; i++)
        {
            thrd_create(&thrds[i], func, &a[i]);
        }
        for(int i = 0; i < 20; i++)
        {
            thrd_join(thrds[i], NULL);
        }
        destroyQueue();
    }
}