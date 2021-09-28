/*
 * Copyright (C) 2015-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#define LOG_TAG "MessageThread"

#include "MessageThread.h"
#include "LogHelper.h"

NAMESPACE_DECLARATION {

void thread_data_t::trampoline(const thread_data_t* t)
{
     thread_func_t f = t->entryFunction;
     void* u = t->userData;

     prctl(PR_SET_NAME, (unsigned long) t->threadName.c_str(), 0, 0, 0);

     delete t;

     f(u);
}

MessageThread::MessageThread(IMessageHandler* runner, const char* name,
                             int priority) :
    mRunner(runner), mName(name), mPriority(priority)
{
    LOGI("@%s:%s prio %d", __FUNCTION__, mName.c_str(), mPriority);
    CLEAR(mThreadId);
}

MessageThread::~MessageThread()
{
    LOGI("@%s:%s prio %d", __FUNCTION__, mName.c_str(), mPriority);
}

status_t MessageThread::requestExitAndWait()
{
    return (pthread_join(mThreadId, nullptr) == 0) ? OK : INVALID_OPERATION;
}

status_t MessageThread::run()
{
    //TODO:add thread policy and priority setting

    status_t ret;
    void *userData;
    thread_func_t entryFunction;

    if (!mName.empty()) {
        thread_data_t* t = new thread_data_t;
        t->threadName = mName;
        t->entryFunction = threadLoop;
        t->userData = this;
        entryFunction = (thread_func_t)&thread_data_t::trampoline;
        userData = t;

        ret = pthread_create(&mThreadId, NULL, entryFunction, userData);
    }
    else
        ret = pthread_create(&mThreadId, NULL, threadLoop, this);

    return (ret == 0) ? OK : NO_INIT;
}

} NAMESPACE_DECLARATION_END
