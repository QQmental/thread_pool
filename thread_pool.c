#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<semaphore.h>


typedef struct thread_pool thread_pool;
typedef struct thread_worker thread_worker;
typedef struct thread_args thread_args;

struct thread_args
{
    thread_args *next;
    void *arg, *ret;
    void *(*task)(void*);
};


struct thread_pool
{
    int MaxWorkerCount, CurrentWorkingCount, MaxArgsLength; // CurrentWorkingCount is shared by workers
    
    thread_args *list_idle_args;

    thread_args *list_ready_args;

    pthread_mutex_t mutex_list_idle_args;

    pthread_mutex_t mutex_list_ready_args;

    pthread_mutex_t mutex_CurrentWorkingCount;

    pthread_cond_t cond_ReadyArgsReleased;

    pthread_cond_t cond_IdleArgsExist;

    pthread_t manager;
};


void AddTask(thread_pool *pool, void*(func)(void*), void *arg);
void* TaskWorker(void *PoolArg);
void* TaskManager(void *PoolArg);
thread_pool* thread_pool_construct(int MaxWorkerCount, int MaxArgsLength);
void *LoopAdd(void *arg);

pthread_mutex_t mutex;

void *LoopAdd(void *arg)
{
    //pthread_mutex_lock(&mutex);

    int i = 0;
    for(; i < 100000 ; i++)
        *(int*)arg += 1;
    //pthread_mutex_unlock(&mutex);
    return NULL;
}

thread_pool* thread_pool_construct(int MaxWorkerCount, int MaxArgsLength)
{
    thread_pool *pool = (thread_pool*)malloc(sizeof(thread_pool));

    if (pool == NULL)
        goto fail;

    pool->MaxWorkerCount = MaxWorkerCount;
    pool->MaxArgsLength = MaxArgsLength;
    pool->CurrentWorkingCount = 0;

    pool->list_idle_args = NULL;
    pool->list_ready_args = NULL;

    int i = 0;

    for(; i < pool->MaxArgsLength ; i++)
    {
        thread_args *NewThreadArg = (thread_args*)malloc(sizeof(thread_args));

        if (NewThreadArg == NULL)
            printf("fail\n");
        
        NewThreadArg->next = pool->list_idle_args;
        
        pool->list_idle_args = NewThreadArg;
    }


    if (pthread_mutex_init(&pool->mutex_list_idle_args, NULL) != 0)
        printf("fail\n");
    if (pthread_mutex_init(&pool->mutex_list_ready_args, NULL) != 0)
        printf("fail\n");    
    if (pthread_mutex_init(&pool->mutex_CurrentWorkingCount, NULL) != 0)
        printf("fail\n");  
    if (pthread_cond_init(&pool->cond_ReadyArgsReleased, NULL) != 0)
        printf("fail\n");
    if (pthread_cond_init(&pool->cond_IdleArgsExist, NULL) != 0)
        printf("fail\n");
   
    pthread_create(&pool->manager, NULL, TaskManager, pool);
    
    for(i = 0 ; i < pool->MaxWorkerCount ; i++)
    {
        pthread_t *thread = (pthread_t*)malloc(sizeof(pthread_t));
        pthread_create(thread, NULL, TaskWorker, pool);
    }
  
    return pool;  

fail:
    return NULL;
}


void* TaskManager(void *PoolArg)
{
    printf("task\n");
    thread_pool *pool = (thread_pool*)PoolArg;
    while(1)
    {
        _sleep(1000);
    }
    
    return NULL;
}


void* TaskWorker(void *PoolArg)
{
    thread_pool *pool = (thread_pool*)PoolArg;

    //jump out this loop when life of this worker is in the end
    while(1)
    {
        pthread_mutex_lock(&pool->mutex_list_ready_args); // trying to take a released task
        while (pool->list_ready_args == NULL)      
            pthread_cond_wait(&pool->cond_ReadyArgsReleased, &pool->mutex_list_ready_args);       

        thread_args *ready_args = pool->list_ready_args; // remove a thread_args from 'pool->list_ready_args'
        pool->list_ready_args = pool->list_ready_args->next;
        pthread_mutex_unlock(&pool->mutex_list_ready_args);
  
        pthread_mutex_lock(&pool->mutex_CurrentWorkingCount); // increase CurrentWorkingCount
        pool->CurrentWorkingCount += 1;
        pthread_mutex_unlock(&pool->mutex_CurrentWorkingCount);
        
        ready_args->ret = ready_args->task(ready_args->arg); // start working

        pthread_mutex_lock(&pool->mutex_CurrentWorkingCount); // decrease CurrentWorkingCount
        pool->CurrentWorkingCount -= 1;
        pthread_mutex_unlock(&pool->mutex_CurrentWorkingCount);
        
        pthread_mutex_lock(&pool->mutex_list_idle_args); // recycle unused thread args
        ready_args->next = pool->list_idle_args;
        pool->list_idle_args = ready_args;
        pthread_cond_broadcast(&pool->cond_IdleArgsExist);
        pthread_mutex_unlock(&pool->mutex_list_idle_args);
    }
    
}

void AddTask(thread_pool *pool, void*(func)(void*), void *arg)
{
    pthread_mutex_lock(&pool->mutex_list_idle_args); // trying to take an idle resourse 
    while(pool->list_idle_args == NULL)
        pthread_cond_wait(&pool->cond_IdleArgsExist, &pool->mutex_list_idle_args);      
    thread_args *ready_args = pool->list_idle_args;
    pool->list_idle_args = pool->list_idle_args->next;
    pthread_mutex_unlock(&pool->mutex_list_idle_args);

    ready_args->task = func;                        // initialize task args
    ready_args->arg = arg;

    pthread_mutex_lock(&pool->mutex_list_ready_args); // put the task into ready list
    int need_signal = (pool->list_ready_args == NULL);
    ready_args->next = pool->list_ready_args;
    pool->list_ready_args = ready_args;
    pthread_cond_broadcast(&pool->cond_ReadyArgsReleased);
    pthread_mutex_unlock(&pool->mutex_list_ready_args);

}



// gcc thread_pool.c -lpthread -g -o thread_pool
// ./thread_pool


int main()
{
    int num = 10;
    thread_pool *pool = thread_pool_construct(3, 10);
    pthread_mutex_init(&mutex, NULL);

    int i ;
    for(i = 0 ; i < 100000 ; i++)
        AddTask(pool, LoopAdd, (void*)&num);

   
    _sleep(10000);
    pthread_join(pool->manager, NULL);


 
    //_sleep(100000);

    
   
    //AddTask(pool, LoopAdd, (void*)&num);
    

}
