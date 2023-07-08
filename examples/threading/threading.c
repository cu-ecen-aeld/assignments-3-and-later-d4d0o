#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
//#define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter

    DEBUG_LOG("I am in a thread!");

    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    usleep ((useconds_t) thread_func_args->wait_to_obtain_ms * 1000);

    // get a lock
    int rc = pthread_mutex_lock(thread_func_args->mutex);
    if ( rc != 0 ) {
        ERROR_LOG("pthread_mutex_lock failed with %d\n", rc);
    }

    usleep ((useconds_t) thread_func_args->wait_to_release_ms * 1000);

    rc = pthread_mutex_unlock(thread_func_args->mutex);
    if ( rc != 0 ) {
        ERROR_LOG("pthread_mutex_unlock failed with %d\n", rc);
    }
    // lock released

    if (0 == rc) {
        thread_func_args->thread_complete_success = true;
    }

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data* thread_param = NULL;

    thread_param = (struct thread_data*) malloc(sizeof(struct thread_data));
    if (NULL == thread_param) {
        ERROR_LOG("No memory allocated for thread_data");
        return false;
    }

    thread_param->mutex = mutex;
    thread_param->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_param->wait_to_release_ms = wait_to_release_ms;
    thread_param->thread_complete_success = false;

    // CREATE A NEW THREAD
    DEBUG_LOG("Let's go in a thread...");
    int rc = pthread_create(thread, NULL, threadfunc, thread_param);
    if ( rc != 0 ) {
        ERROR_LOG("pthread_create failed with error %d creating thread id %p\n", rc, thread);
        return false;
    }

    return true;
}

