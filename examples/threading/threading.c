#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    if(!thread_param) {
        ERROR_LOG("thread_param is NULL");
        return NULL;
    }

    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    thread_func_args->thread_complete_success = false;

    if(thread_func_args->my_mutex == NULL) {
        ERROR_LOG("mutex pointer is NULL");
        return thread_param;
    }

    struct timespec req;
    struct timespec rem;

    DEBUG_LOG("Waiting %d ms before obtaining mutex",
              thread_func_args->wait_to_obtain_ms);

    /* Convert ms to sec + nsec */
    req.tv_sec  = thread_func_args->wait_to_obtain_ms / 1000;
    req.tv_nsec = (thread_func_args->wait_to_obtain_ms % 1000) * 1000000;

    while (nanosleep(&req, &rem) == -1) {
        if (errno == EINTR) {
            req = rem;   // sleep remaining time
        } else {
            ERROR_LOG("nanosleep before lock failed");
            return thread_param;
        }
    }

    DEBUG_LOG("Attempting to lock mutex");

    int rc = pthread_mutex_lock(thread_func_args->my_mutex);
    if (rc != 0) {
        ERROR_LOG("pthread_mutex_lock failed, rc=%d", rc);
        return thread_param;
    }

    DEBUG_LOG("Mutex locked, holding for %d ms",
              thread_func_args->wait_to_release_ms);

    /* Convert ms to sec + nsec */
    req.tv_sec  = thread_func_args->wait_to_release_ms / 1000;
    req.tv_nsec = (thread_func_args->wait_to_release_ms % 1000) * 1000000;

    while (nanosleep(&req, &rem) == -1) {
        if (errno == EINTR) {
            req = rem;   // sleep remaining time
        } else {
            ERROR_LOG("nanosleep while holding mutex failed");
            pthread_mutex_unlock(thread_func_args->my_mutex);
            return thread_param;
        }
    }

    DEBUG_LOG("Releasing mutex");

    rc = pthread_mutex_unlock(thread_func_args->my_mutex);
    if (rc != 0) {
        ERROR_LOG("pthread_mutex_unlock failed, rc=%d", rc);
        return thread_param;
    }

    thread_func_args->thread_complete_success = true;
    DEBUG_LOG("Thread completed successfully");

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,
                                 int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    if(!thread || !mutex) return false;
    if(wait_to_obtain_ms < 0 || wait_to_release_ms < 0) return false;

    struct thread_data* thread_param = (struct thread_data*) malloc(sizeof(*thread_param));
    if(!thread_param) return false;

    thread_param->my_mutex = mutex;
    thread_param->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_param->wait_to_release_ms = wait_to_release_ms;
    thread_param->thread_complete_success = false;

    int ret = pthread_create(thread, NULL, threadfunc, thread_param);
    if(ret != 0) {
        errno = ret;
        perror("pthread_create");
        free(thread_param);
        return false;
    }

    return true;
}
