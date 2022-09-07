#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "codec_test.h"
#include "script.h"
#include <thread>

#define DBG 1
#if DBG
#define LOGINFO(args...) printf(args)
#define LOGERR(args...) fprintf(stderr, args)
#else
#define LOGINFO(args...)
#define LOGERR(args...)
#endif

struct test_context {
    int sync;
};

static void *test_thread_loop(void *context)
{
    struct test_context *pcontext = (struct test_context *)context;
    int sync = 0;

    if (pcontext)
        sync = pcontext->sync;
    if(sync) {
        LOGINFO ("test_thread_loop:start sync test\n");
        rec_play_test_sync();
    } else {
        LOGINFO ("test_thread_loop:start async test\n");
        rec_play_test_async();
    }
    LOGINFO ("test_thread_loop:test exit\n");
    return 0;
}

int main(int argc, char **argv)
{
    struct test_context context = {0};
    pthread_t ptest_thread;
    int err;

    LOGINFO("enter audio device test....\n");
    if(argc > 1) {
        context.sync = !strcmp(argv[1], "case2");
    }
    set_exit(0);
    err = pthread_create(&ptest_thread,
                         (const pthread_attr_t *) NULL,
                         test_thread_loop,
                         &context);
    if (err != 0) {
        LOGERR("pthread_create() returned %d %s", err, strerror(err));
        return -1;
    } 
    /* wait 20s */
    usleep(1000*1000*20);
    set_exit(1);
    LOGERR("pthread_join()....");
    err = pthread_join(ptest_thread, NULL);
    if (err != 0) {
        LOGERR("pthread_join() returned %d %s", err, strerror(err));
    }
    LOGINFO("exit audio device test....\n");
    return 0;
}
