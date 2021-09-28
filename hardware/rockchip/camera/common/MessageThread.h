/*
 * Copyright (C) 2013-2017 Intel Corporation
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
#ifndef CAMERA3_HAL_IMESSAGETHREAD_H_
#define CAMERA3_HAL_IMESSAGETHREAD_H_
#include <utils/Errors.h>
#include <pthread.h>
#include <string>
#include <sys/prctl.h>
#include <string.h>

#define PRIORITY_CAMERA (-10)

NAMESPACE_DECLARATION {

/* Abstraction of MessageThread */
class IMessageHandler
{
public:
    virtual ~IMessageHandler() { };
    virtual void messageThreadLoop(void) = 0;
};

typedef void* (*thread_func_t)(void*);

struct thread_data_t {
   thread_func_t   entryFunction;
   void* userData;
   std::string threadName;

   // set thread name with prctl.
   static void trampoline(const thread_data_t* t);
};

class MessageThread {
public:
    MessageThread(IMessageHandler* runner, const char* name,
                  int priority = PRIORITY_CAMERA);
    virtual ~MessageThread();

    // Start the thread in threadLoop() which needs to be implemented.
    status_t    run();

    // Call requestExit() and wait until this object's thread exits.
    status_t    requestExitAndWait();

private:
    // prevent copy constructor and assignment operator
    MessageThread(const MessageThread& other);
    MessageThread& operator=(const MessageThread& other);

    static void* threadLoop(void* This)  {
        ((MessageThread *)This)->mRunner->messageThreadLoop();
        return nullptr;
    };

    IMessageHandler* mRunner;
    std::string mName;
    int mPriority;
    pthread_t mThreadId;
};

} NAMESPACE_DECLARATION_END
#endif /* CAMERA3_HAL_IMESSAGETHREAD_H_ */

