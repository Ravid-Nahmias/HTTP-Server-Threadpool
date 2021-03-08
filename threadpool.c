#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/**
 * threadpool.h
 *
 * This file declares the functionality associated with
 * your implementation of a threadpool.
 */

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void *do_work(void *p)
{
    // The work function of the thread

    threadpool *t = (threadpool *)p; // casting of the argument recieved
    int check;
    while (1) // endless loop
    {
        check = pthread_mutex_lock(&t->qlock);
        if (check != 0)
        {
            // systemError("lock");
            return NULL;
        }
        if (t->shutdown == 1) // if the flag raised up
        {
            check = pthread_mutex_unlock(&t->qlock);
            // if (check != 0)
            //     systemError("unlock");
            return NULL;
        }
        else if (t->qsize == 0)
        { // if the queue is empty we wait
            check = pthread_cond_wait(&t->q_not_empty, &t->qlock);
            if (check != 0)
            {
                // systemError("wait");
                return NULL;
            }
        }
        if (t->shutdown == 1)   // if the shutdown flag raised while waiting - no work should be execute
        {
            check = pthread_mutex_unlock(&t->qlock); // if the flag raised up while he was on wait status
            // if (check != 0)
            //     systemError("unlock");
            return NULL;
        }
        // take the first element from the queue
        work_t *cur = t->qhead;
        if (t->qhead != NULL) // if the list is not empty
        {
            t->qhead = t->qhead->next;
            t->qsize--;
        }

        if (t->qhead == t->qtail) // end of the list
            t->qtail = NULL;

        if (t->qsize == 0 && t->dont_accept == 1)
        { // if the queue is empty and the flag is up s
            check = pthread_cond_signal(&t->q_empty);
            if (check != 0)
            {
                // systemError("signal");
                return NULL;
            }
        }
        check = pthread_mutex_unlock(&t->qlock);
        if (check != 0)
        {
            // systemError("unlock");
            return NULL;
        }
        if (cur != NULL)    // if the list was not empty
        {
            cur->routine(cur->arg); //call the thread routine
            free(cur);
        }
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

threadpool *create_threadpool(int num_threads_in_pool)
{
    /** create_threadpool creates a fixed-sized thread
 * pool.  If the function succeeds, it returns a (non-NULL)
 * "threadpool", else it returns NULL.
 */
    if (num_threads_in_pool <= 0 || num_threads_in_pool > MAXT_IN_POOL)
    { // check valid input
        // commandError();
        return NULL;
    }
    threadpool *tp = (threadpool *)calloc(1, sizeof(threadpool));
    if (tp == NULL)
    {
        // systemError("calloc");
        return NULL;
    }
    //initialize the threadpool structure
    tp->num_threads = num_threads_in_pool;
    tp->qsize = 0;
    tp->qhead = NULL;
    tp->qtail = NULL;
    tp->shutdown = 0;
    tp->dont_accept = 0;

    int check = 0;
    check = pthread_mutex_init(&tp->qlock, NULL); //initialized mutex
    if (check != 0)
    {
        //systemError("init mutex");
        return NULL;
    }
    check = pthread_cond_init(&tp->q_not_empty, NULL); // initialize conditional variables
    if (check != 0)
    {
        //systemError("init cond q_not_empty");
        return NULL;
    }
    check = pthread_cond_init(&tp->q_empty, NULL); // initialize conditional variables
    if (check != 0)
    {
        //systemError("init cond q_empty");
        return NULL;
    }

    //create the threads, the thread init function is do_work and its argument is the initialized threadpool.
    tp->threads = (pthread_t *)calloc(tp->num_threads, sizeof(pthread_t));
    if (tp->threads == NULL)
    {
        //systemError("calloc");
        return NULL;
    }
    //pthread_t tids[tp->num_threads];
    for (int i = 0; i < num_threads_in_pool; i++)
    {
        check = pthread_create(tp->threads + i, NULL, do_work, tp);
        if (check)
        {
            //systemError("pthread create");
            return NULL;
        }
    }

    return tp;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void dispatch(threadpool *from_me, dispatch_fn dispatch_to_here, void *arg)
{
    /**
 * dispatch enter a "job" of type work_t into the queue.
 * when an available thread takes a job from the queue, it will
 * call the function "dispatch_to_here" with argument "arg".
 */
    int check = pthread_mutex_lock(&from_me->qlock);
    if (check != 0)
        return;

    if (from_me->dont_accept == 1)
    {
        printf("can't add more jobs\n");
        return;
    }

    work_t *wrk = (work_t *)calloc(1, sizeof(work_t)); //create and init work_t element
    if (wrk == NULL)
    {
        //systemError("calloc");
        return;
    }
    wrk->arg = arg;
    wrk->routine = dispatch_to_here;

    if (from_me->qhead == NULL)
    { // if the list is empty
        from_me->qhead = wrk;
        from_me->qtail = wrk;
    }
    else
    {
        from_me->qtail->next = wrk;
        from_me->qtail = wrk;
    }

    from_me->qsize++;
    check = pthread_mutex_unlock(&from_me->qlock);
    if (check != 0)
    {
        //systemError("unlock");
        return;
    }
    check = pthread_cond_signal(&from_me->q_not_empty);
    if (check != 0)
    {
        //systemError("signal");
        return;
    }
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void destroy_threadpool(threadpool *destroyme)
{ /**
 * destroy_threadpool kills the threadpool, causing
 * all threads in it to commit suicide, and then
 * frees all the memory associated with the threadpool.
 */
    int check = 0;
    check = pthread_mutex_lock(&destroyme->qlock);
    if (check != 0)
    {
        //systemError("lock");
        return;
    }
    destroyme->dont_accept = 1; // raising the flag
    if (destroyme->qsize != 0)
    { // the queue is not empty
        check = pthread_cond_wait(&destroyme->q_empty, &destroyme->qlock);
        if (check != 0)
        {
            //systemError("wait cond q_empty");
            return;
        }
    }
    destroyme->shutdown = 1;
    check = pthread_mutex_unlock(&destroyme->qlock);
    if (check != 0)
    {
        //systemError("unlock");
        return;
    }
    check = pthread_cond_broadcast(&destroyme->q_not_empty); //wake up all the sleeping threads
    if (check != 0)
    {
        //systemError("broadcast");
        return;
    }
    void *retval;
    for (int i = 0; i < destroyme->num_threads; i++)
    {
        check = pthread_join(destroyme->threads[i], &retval);
        if (check != 0)
        {
            //systemError("join");
            return;
        }
    }
    if (destroyme->threads != NULL)
        free(destroyme->threads);
    if (destroyme != NULL)
        free(destroyme);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
