#ifndef TVINPUT_BUFFER_DATA_H_
#define TVINPUT_BUFFER_DATA_H_
#include <cutils/native_handle.h>
#include <string>
#include "time.h"
#include "log/log.h"

typedef enum request_type {
   RENDER_TYPE,
   ENCODER_TYPE
} request_type_t;

typedef enum bufer_type {
   INPUT_BUFFER,
   OUTPUT_BUFFER
} bufer_type_t;

typedef enum buffer_status {
    /**
     * The buffer is in a normal state, and can be used after waiting on its
     * sync fence.
     */
    BUFFER_STATUS_OK = 0,

    /**
     * The buffer does not contain valid data, and the data in it should not be
     * used. The sync fence must still be waited on before reusing the buffer.
     */
    BUFFER_STATUS_ERROR = 1

} buffer_status_t;

typedef struct stream_config {
    int streamtype;
    int width;
    int height;
    int format;
    int usage;
} stream_config_t;

typedef struct rt_stream_buffer {

    stream_config_t stream;

    buffer_handle_t buffer;

    int status;

    int acquire_fence;

    int release_fence;

} rt_stream_buffer_t;

class  Autotime {
    public:
        inline explicit Autotime(std::string name) {
            struct timeval  start;
            gettimeofday(&start, NULL);
            mStartTime = 1000000 *start.tv_sec  + start.tv_usec;
            mName = name;
        }
        inline ~Autotime() {
            struct timeval  end;
            gettimeofday(&end, NULL);
            int64_t endtime = 1000000 *end.tv_sec  + end.tv_usec;
            int64_t diff = endtime - mStartTime;
            ALOGD("call %s,time:%lld (us)",mName.data(), (long long)diff);
        }

    private:
        int64_t mStartTime;
        std::string mName;
        // Cannot be copied or moved - declarations only
        Autotime(const Autotime&);
        Autotime& operator=(const Autotime&);
};
#endif