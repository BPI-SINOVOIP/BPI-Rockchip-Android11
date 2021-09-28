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

#include "MessageQueue.h"

namespace android {

template <class MessageType, class MessageId>
MessageQueue<MessageType, MessageId>::MessageQueue(char const* name, int numReply)
    : mName(name),
      mNumReply(numReply),
      mReplyMutex(nullptr),
      mReplyCondition(nullptr),
      mReplyStatus(nullptr)
{
    if (mNumReply > 0) {
        mReplyMutex = new std::mutex[numReply];
        mReplyCondition = new std::condition_variable[numReply];
        mReplyStatus = new status_t[numReply];
    }
}

template <class MessageType, class MessageId>
MessageQueue<MessageType, MessageId>::~MessageQueue()
{
    if (size() > 0) {
        // The last message a thread should receive is EXIT.
        // If for some reason a thread is sent a message after
        // the thread has exited then there is a race condition
        // or design issue.
     //   LOGE("Camera_MessageQueue error: %s queue should be empty. Find the bug.", mName);
    }

    if (mNumReply > 0) {
        delete [] mReplyMutex;
        mReplyMutex = nullptr;
        delete [] mReplyCondition;
        mReplyCondition = nullptr;
        delete [] mReplyStatus;
        mReplyStatus = nullptr;
    }
}

template <class MessageType, class MessageId>
status_t MessageQueue<MessageType, MessageId>::send(MessageType *msg,
                                             MessageId replyId)
{
    status_t status = NO_ERROR;
    bool notDefReplyId = (replyId != (MessageId)-1);

    // someone is misusing the API. replies have not been enabled
    if (notDefReplyId && mNumReply == 0) {
        ALOGE("Camera_MessageQueue error: %s replies not enabled\n", mName);
        return BAD_VALUE;
    }


    if (replyId < -1 || replyId >= mNumReply) {
        ALOGE("Camera_MessageQueue error: incorrect replyId: %d\n", replyId);
        return BAD_VALUE;
    }

    {
        std::lock_guard<std::mutex> l(mQueueMutex);
        MessageType data = *msg;
        mList.push_front(data);
        if (notDefReplyId) {
            mReplyStatus[replyId] = WOULD_BLOCK;
        }
        mQueueCondition.notify_one();
    }

    if (notDefReplyId && replyId >= 0) {
        std::unique_lock<std::mutex> lk(mReplyMutex[replyId]);
        while (mReplyStatus[replyId] == WOULD_BLOCK) {
            mReplyCondition[replyId].wait(lk);
            // wait() should never complete without a new status having
            // been set, but for diagnostic purposes let's check it.
            if (mReplyStatus[replyId] == WOULD_BLOCK) {
                ALOGE("Camera_MessageQueue - woke with WOULD_BLOCK\n");
            }
        }
        status = mReplyStatus[replyId];
    }

    return status;
}

template <class MessageType, class MessageId>
status_t MessageQueue<MessageType, MessageId>::remove(MessageId id,
                                             std::vector<MessageType> *vect)
{
    status_t status = NO_ERROR;
    if(isEmpty())
        return status;

    {
        std::lock_guard<std::mutex> l(mQueueMutex);
        typename std::list<MessageType>::iterator it = mList.begin();
        while (it != mList.end()) {
            MessageType msg = *it;
            if (msg.id == id) {
                if (vect) {
                    vect->push_back(msg);
                }
                it = mList.erase(it); // returns pointer to next item in list
            } else {
                it++;
            }
        }
    }

    // unblock caller if waiting
    if (mNumReply > 0) {
        reply(id, INVALID_OPERATION);
    }

    return status;
}

template <class MessageType, class MessageId>
status_t MessageQueue<MessageType, MessageId>::receive(MessageType *msg,
            unsigned int timeout_ms)
{
    status_t status = NO_ERROR;
    std::unique_lock<std::mutex> l(mQueueMutex);

    while (isEmptyLocked()) {
        if (timeout_ms) {
            mQueueCondition.wait_for(l, std::chrono::milliseconds(timeout_ms));
        } else {
            mQueueCondition.wait(l);
        }

        if (isEmptyLocked()) {
            ALOGE("Camera_MessageQueue - woke with mCount == 0\n");
        }
    }

    *msg = *(--mList.end());
    mList.erase(--mList.end());
    return status;
}

template <class MessageType, class MessageId>
void MessageQueue<MessageType, MessageId>::reply(MessageId replyId, status_t status)
{
    if (replyId < 0 || replyId > mNumReply) {
       ALOGE("Camera_MessageQueue error: incorrect replyId\n");
        return;
    }

    std::lock_guard<std::mutex> l(mReplyMutex[replyId]);
    mReplyStatus[replyId] = status;
    mReplyCondition[replyId].notify_one();
}

template <class MessageType, class MessageId>
bool MessageQueue<MessageType, MessageId>::isEmpty()
{
    std::lock_guard<std::mutex> l(mQueueMutex);
    return isEmptyLocked();
}

template <class MessageType, class MessageId>
int MessageQueue<MessageType, MessageId>::size()
{
    std::lock_guard<std::mutex> l(mQueueMutex);
    return sizeLocked();
}

}
