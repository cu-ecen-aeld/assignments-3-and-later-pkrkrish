#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // Added for strerror used in start_thread_obtaining_mutex
#include <errno.h>  // Added for error handling

// Helper function to sleep in milliseconds using usleep
// Moved to top to ensure it is defined before use
static void msleep(int ms)
{
    // Check for negative input to prevent unintended behavior
    if (ms > 0) {
        usleep((useconds_t)ms * 1000); 
    }
}

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    // The cast is correct, but ensure it's done immediately.
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    
    // Set to false initially, to be safe in case of early exit.
    thread_func_args->thread_complete_success = false;

    // 1. Wait to obtain mutex
    DEBUG_LOG("Waiting for %d ms before attempting to lock.", thread_func_args->wait_to_obtain_ms);
    msleep(thread_func_args->wait_to_obtain_ms);

    // 2. Obtain mutex
    DEBUG_LOG("Attempting to lock mutex...");
    if (pthread_mutex_lock(thread_func_args->mutex) != 0) {
        ERROR_LOG("Failed to lock mutex");
        // Return thread_param (thread_func_args) to allow the joiner to free memory
        return thread_param; 
    }
    DEBUG_LOG("Mutex locked successfully.");

    // 3. Wait while holding mutex
    DEBUG_LOG("Holding lock for %d ms...", thread_func_args->wait_to_release_ms);
    msleep(thread_func_args->wait_to_release_ms);

    // 4. Release mutex
    if (pthread_mutex_unlock(thread_func_args->mutex) != 0) {
        ERROR_LOG("Failed to unlock mutex");
        // NOTE: The mutex is already unlocked if we reach this point, but this path handles an unlock failure.
        return thread_param; 
    }
    DEBUG_LOG("Mutex unlocked successfully. Thread complete.");

    // Set success flag
    thread_func_args->thread_complete_success = true;
    
    // Return the thread_data structure pointer
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,
                                  int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data *new_thread_data = NULL;
    int rc;

    // 1. Allocate memory for thread_data
    new_thread_data = (struct thread_data *)malloc(sizeof(struct thread_data));
    if (new_thread_data == NULL) {
        ERROR_LOG("Failed to allocate memory for thread_data");
        return false;
    }

    // 2. Setup mutex and wait arguments
    new_thread_data->mutex = mutex;
    new_thread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    new_thread_data->wait_to_release_ms = wait_to_release_ms;
    new_thread_data->thread_complete_success = false; // Initialize to false

    // 3. Create the thread
    rc = pthread_create(thread, 
                        NULL,             // Default attributes
                        threadfunc,       // Entry point function
                        (void *)new_thread_data); // Pass thread_data structure as argument

    if (rc != 0) {
        // Use strerror to provide a meaningful message if thread creation fails
        ERROR_LOG("Failed to create thread: %s", strerror(rc)); 
        // Must free the memory if thread creation fails
        free(new_thread_data); 
        return false;
    }
    
    // Thread created successfully
    return true;
}
