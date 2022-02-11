/*
 * Copyright 2021 Rockchip Electronics Co. LTD
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
 *
 * author: kevin.chen@rock-chips.com
 */

#ifndef __Q_LIST_H__
#define __Q_LIST_H__

#include <pthread.h>

class Mutex;
class Condition;

/*
 * for shorter type name and function name
 */
class Mutex
{
public:
    Mutex();
    ~Mutex();

    void lock();
    void unlock();
    int  trylock();

    class Autolock
    {
    public:
        inline Autolock(Mutex& mutex) : mLock(mutex)  { mLock.lock(); }
        inline Autolock(Mutex* mutex) : mLock(*mutex) { mLock.lock(); }
        inline ~Autolock() { mLock.unlock(); }
    private:
        Mutex& mLock;
    };

private:
    friend class Condition;

    pthread_mutex_t mMutex;

    Mutex(const Mutex &);
    Mutex &operator = (const Mutex&);
};

inline Mutex::Mutex()
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mMutex, &attr);
    pthread_mutexattr_destroy(&attr);
}
inline Mutex::~Mutex()
{
    pthread_mutex_destroy(&mMutex);
}
inline void Mutex::lock()
{
    pthread_mutex_lock(&mMutex);
}
inline void Mutex::unlock()
{
    pthread_mutex_unlock(&mMutex);
}
inline int Mutex::trylock()
{
    return pthread_mutex_trylock(&mMutex);
}

typedef Mutex::Autolock AutoMutex;


/*
 * for shorter type name and function name
 */
class Condition
{
public:
    Condition();
    Condition(int type);
    ~Condition();
    int wait(Mutex& mutex);
    int wait(Mutex* mutex);
    int timedwait(Mutex& mutex, int timeout);
    int timedwait(Mutex* mutex, int timeout);
    int signal();

private:
    pthread_cond_t mCond;
};

inline Condition::Condition()
{
    pthread_cond_init(&mCond, NULL);
}
inline Condition::~Condition()
{
    pthread_cond_destroy(&mCond);
}
inline int Condition::wait(Mutex& mutex)
{
    return pthread_cond_wait(&mCond, &mutex.mMutex);
}
inline int Condition::wait(Mutex* mutex)
{
    return pthread_cond_wait(&mCond, &mutex->mMutex);
}
inline int Condition::timedwait(Mutex& mutex, int timeout)
{
    return timedwait(&mutex, timeout);
}
inline int Condition::timedwait(Mutex* mutex, int timeout)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME_COARSE, &ts);

    ts.tv_sec += timeout / 1000;
    ts.tv_nsec += (timeout % 1000) * 1000000;
    /* Prevent the out of range at nanoseconds field */
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;

    return pthread_cond_timedwait(&mCond, &mutex->mMutex, &ts);
}
inline int Condition::signal()
{
    return pthread_cond_signal(&mCond);
}

// desctructor of list node
typedef void *(*node_destructor)(void *);
struct list_node;

class QList {
public:
    QList(node_destructor func = NULL);
    ~QList();

    // for FIFO or FILO implement
    // adding functions support simple structure like C struct or C++ class pointer,
    // do not support C++ object
    int add_at_head(void *data, int size);
    int add_at_tail(void *data, int size);
    // deleting function will copy the stored data to input pointer with size as size
    // if NULL is passed to deleting functions, the node will be delete directly
    int del_at_head(void *data, int size);
    int del_at_tail(void *data, int size);

    // for status check
    int list_is_empty();
    int list_size();

    // for vector implement - not implemented yet
    // adding function will return a key
    int add_by_key(void *data, int size, int *key);
    int del_by_key(void *data, int size, int key);
    int show_by_key(void *data, int key);

    int flush();

    // open lock function for external combination usage
    void lock();
    void unlock();
    int trylock();

    // open lock function for external auto lock
    Mutex *mutex();

    void wait();
    int wait(int timeout);
    void signal();

private:
    Mutex                   mMutex;
    Condition               mCondition;

    node_destructor         destroy;
    struct list_node        *head;
    int                     count;
    static int              keys;
    static int              get_key();

    QList(const QList &);
    QList &operator=(const QList &);
};

#endif // __Q_LIST_H__
