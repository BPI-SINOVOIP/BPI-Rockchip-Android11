#ifndef _LIBAIO_H
#define _LIBAIO_H

// Don't include the actual header uapi/aio_abi.h
// since libaio redefines the structs for some reason.
// Instead override those definitions with the ones below.
#define __LINUX__AIO_ABI_H

#include <stdint.h>

#if __UINTPTR_MAX__ == UINT64_MAX
#define PADDED_PTR(x, y) x
#elif __UINTPTR_MAX__ == UINT32_MAX
#define PADDED_PTR(x, y) x; unsigned y
#endif

struct io_iocb_common {
    PADDED_PTR(void *buf, __pad1);
    __u64 nbytes;
    __s64    offset;
    __u64    reserved2;
    __u32    flags;
    __u32    resfd;
};

struct iocb {
    PADDED_PTR(void *data, __pad1);
    __u32 key;
    __u32 aio_rw_flags;
    __u16 aio_lio_opcode;
    __s16 aio_reqprio;
    __u32 aio_fildes;
    union {
        struct io_iocb_common c;
    } u;
};

struct io_event {
    PADDED_PTR(void *data, __pad1);
    PADDED_PTR(struct iocb *obj, __pad2);
    __s64 res;
    __s64 res2;
};

typedef unsigned long io_context_t;
typedef io_context_t aio_context_t;

#include <asyncio/AsyncIO.h>

#define IO_CMD_PREAD 0
#define IO_CMD_PWRITE 1
#define IO_CMD_FSYNC 2
#define IO_CMD_FDSYNC 3
#define IO_CMD_POLL 5
#define IO_CMD_NOOP 6
#define IO_CMD_PREADV 7
#define IO_CMD_PWRITEV 8

typedef void (*io_callback_t)(io_context_t ctx, struct iocb *iocb, long res, long res2);

static inline int redirect_error(int ret) {
    return ret == -1 ? -errno : ret;
}

// libaio doesn't follow syscall convention, so errors are returned
// as negative values and errno isn't used.

static inline int libaio_setup(int maxevents, io_context_t *ctxp) {
    int ret = io_setup(maxevents, ctxp);
    return redirect_error(ret);
}

static inline int libaio_destroy(io_context_t ctx) {
    int ret = io_destroy(ctx);
    return redirect_error(ret);
}

static inline int libaio_submit(io_context_t ctx, long nr, struct iocb *ios[]) {
    int ret = io_submit(ctx, nr, ios);
    return redirect_error(ret);
}

static inline int libaio_cancel(io_context_t ctx, struct iocb *iocb, struct io_event *evt) {
    int ret = io_cancel(ctx, iocb, evt);
    return redirect_error(ret);
}

static inline int libaio_getevents(io_context_t ctx_id, long min_nr, long nr, struct io_event *events, struct timespec *timeout) {
    int ret = io_getevents(ctx_id, min_nr, nr, events, timeout);
    return redirect_error(ret);
}

static inline void io_set_callback(struct iocb *iocb, io_callback_t cb)
{
    iocb->data = (void *)cb;
}

static inline int io_queue_init(int maxevents, io_context_t *ctxp) {
    memset(ctxp, 0, sizeof(*ctxp));
    return libaio_setup(maxevents, ctxp);
}

// Override the system calls with their libaio versions.

#define io_setup(a, b) libaio_setup(a, b)
#define io_destroy(a) libaio_destroy(a)
#define io_submit(a, b, c) libaio_submit(a, b, c)
#define io_cancel(a, b, c) libaio_cancel(a, b, c)
#define io_getevents(a, b, c, d, e) libaio_getevents(a, b, c, d, e)

#define io_queue_release(a) io_destroy(a)

#endif
