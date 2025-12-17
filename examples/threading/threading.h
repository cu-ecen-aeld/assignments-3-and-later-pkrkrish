#include <stdbool.h>
#include <pthread.h>

/**
 * This structure should be dynamically allocated and passed as
 * an argument to your thread using pthread_create.
 * It should be returned by your thread so it can be freed by
 * the joiner thread.
 */
struct thread_data{
    // Pointer to the mutex to be used by the thread
    pthread_mutex_t *mutex; 
    // Wait time before attempting to obtain the mutex (in ms)
    int wait_to_obtain_ms;
    // Wait time after obtaining the mutex (in ms)
    int wait_to_release_ms;

    /**
     * Set to true if the thread completed with success, false
     * if an error occurred.
     */
    bool thread_complete_success;
};


/**
* Start a thread which sleeps @param wait_to_obtain_ms number of milliseconds, then obtains the
* mutex in @param mutex, then holds for @param wait_to_release_ms milliseconds, then releases.
* ... (Rest of the original comments)
*/
bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms);
