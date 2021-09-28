/*
 * Copyright (C) 2011-2017 Intel Corporation
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

#ifndef CAMERA3_HAL_MESSAGE_QUEUE_H_
#define CAMERA3_HAL_MESSAGE_QUEUE_H_

#include "Errors.h"
//#include <Utils.h>
#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>
//#include "LogHelper.h"
#include <chrono>
#include "log/log.h"
#include <utils/Errors.h>

// By default MessageQueue::receive() waits infinitely for a new message
#define MESSAGE_QUEUE_RECEIVE_TIMEOUT_MSEC_INFINITE 0

namespace android {

template <class MessageType, class MessageId>
class MessageQueue {

    // constructor / destructor
public:
    explicit MessageQueue(char const* name, // for debugging
            int numReply = 0);    // set numReply only if you need synchronous messages

    ~MessageQueue();

    // public methods
public:

    // Push a message onto the queue. If replyId is not -1 function will block until
    // the caller is signalled with a reply. Caller is unblocked when reply method is
    // called with the corresponding message id.
    status_t send(MessageType *msg, MessageId replyId = (MessageId) -1);

    status_t remove(MessageId id, std::vector<MessageType> *vect = nullptr);

    // Pop a message from the queue
    status_t receive(MessageType *msg,
            unsigned int timeout_ms = MESSAGE_QUEUE_RECEIVE_TIMEOUT_MSEC_INFINITE);

    // Unblock the caller of send and indicate the status of the received message
    void reply(MessageId replyId, status_t status);

    // Return true if the queue is empty
    bool isEmpty();

    int size();

private:
    // prevent copy constructor and assignment operator
    MessageQueue(const MessageQueue& other);
    MessageQueue& operator=(const MessageQueue& other);

    // Return true if the queue is empty, must be called
    // with mQueueMutex taken
    inline bool isEmptyLocked() { return sizeLocked() == 0; }

    inline int sizeLocked() { return mList.size(); }

    const char *mName;
    std::mutex mQueueMutex; /* protects mList */
    std::condition_variable mQueueCondition;
    std::list<MessageType> mList;

    const int mNumReply;
    std::mutex *mReplyMutex; /* protects mReplayStatus */
    std::condition_variable* mReplyCondition;
    status_t *mReplyStatus;

}; // class MessageQueue

}

#include "MessageQueue.cpp"

#endif // _MESSAGE_QUEUE
