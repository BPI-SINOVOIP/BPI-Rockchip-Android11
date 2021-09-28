#ifndef __CUTILS_THREADS_H__
#define __CUTILS_THREADS_H__

#include <pthread.h>

typedef struct {
    pthread_mutex_t   lock;
    int               has_tls;
    pthread_key_t     tls;
} thread_store_t;

#define THREAD_STORE_INITIALIZER  { PTHREAD_MUTEX_INITIALIZER, 0, 0 }

extern "C" {

typedef void (*thread_store_destruct_t)(void* value);

void* thread_store_get(thread_store_t* store);

void thread_store_set(thread_store_t* store,
                      void* value,
                      thread_store_destruct_t destroy);

pid_t gettid();

}

#endif
