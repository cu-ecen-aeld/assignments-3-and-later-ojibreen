#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
// #define DEBUG_LOG(msg,...)
#define DEBUG_LOG(msg,...) printf("[DEBUG] threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("[ERROR] threading: " msg "\n" , ##__VA_ARGS__)


void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    bool status = false;
    struct thread_data* args = (struct thread_data *) thread_param;

    if (usleep(args->wait_to_obtain_ms) == 0) {
        int rc = pthread_mutex_lock(args->mutex);
        if (rc == 0) {
            if (usleep(args->wait_to_release_ms) == 0) {
                status = (pthread_mutex_unlock(args->mutex) == 0);
            } else {
                ERROR_LOG("Failure to release mutex");
            }
        } else {
            ERROR_LOG("Failure to obtain mutex");
        }
    } else {
        ERROR_LOG("Failure to incoke wait");
    }

    args->thread_complete_success = status;
    if (!status) {
        ERROR_LOG("FAILURE: %d", status);
    } else {
        DEBUG_LOG("Unlocked mutex");
    }
    return args;
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
    struct thread_data* args = malloc(sizeof *args);
    args->mutex = mutex;
    args->thread_id = thread;
    args->wait_to_obtain_ms = wait_to_obtain_ms;
    args->wait_to_release_ms = wait_to_release_ms;

    int rc_th = pthread_create(args->thread_id, NULL, &threadfunc, (void *)args);
    if (rc_th != 0) {
        ERROR_LOG("Failure to create thread");

        free(args);
        return false;
    }

    return true;
}
