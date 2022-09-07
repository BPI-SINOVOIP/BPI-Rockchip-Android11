#ifndef __CAMERA_TEST_H__
#define __CAMERA_TEST_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <getopt.h> /* getopt_long() */
#include <fcntl.h> /* low-level i/o */
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
//#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>

#include <linux/videodev2.h>
#include <linux/fb.h>
#include <linux/version.h>
#include "../test_case.h"
#include "../language.h"
#include "../display_callback.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define FMT_NUM_PLANES 1

#define BUFFER_COUNT 4

enum io_method {
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
    IO_METHOD_DMABUF,
};
struct buffer {
    void *start;
    size_t length;
    struct v4l2_buffer v4l2_buf;
};

// the func is a while loop func , MUST  run in a single thread.
//return value: 0 is ok ,-1 erro

struct camera_msg {
    struct testcase_info *tc_info;
    int result;
    int id;
    int x;
    int y;
    int w;
    int h;
};

void* camera_test(void *argc, display_callback *hook);
//return value: 0 is ok ,-1 erro
extern int stopCameraTest();
extern void finishCameraTest();
extern int Camera_Click_Event(int x,int y);
#endif
