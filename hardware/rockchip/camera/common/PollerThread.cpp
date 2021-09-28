/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#define LOG_TAG "PollerThread"

#include <unistd.h>
#include <fcntl.h>
#include "PollerThread.h"
#include "LogHelper.h"


NAMESPACE_DECLARATION {

PollerThread::PollerThread(const char* name,
                            int priority):
    mName(name),
    mPriority(priority),
    mThreadRunning(false),
    mMessageQueue("PollThread", static_cast<int>(MESSAGE_ID_MAX)),
    mMessageThread(new MessageThread(this, mName.c_str(), mPriority)),
    mListener (nullptr),
    mEvents(POLLPRI | POLLIN | POLLERR)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    mFlushFd[0] = -1;
    mFlushFd[1] = -1;
    mPid = getpid();
    mMessageThread->run();
}

PollerThread::~PollerThread()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    close(mFlushFd[0]);
    close(mFlushFd[1]);
    mFlushFd[0] = -1;
    mFlushFd[1] = -1;

    // detach Listener
    mListener = nullptr;

    if (mMessageThread != nullptr) {
        mMessageThread.reset();
        mMessageThread = nullptr;
    }
}

/**
 * init()
 * initialize flush file descriptors and other class members
 *
 * \param devices to poll.
 * \param observer event listener
 * \param events the poll events (bits)
 * \param makeRealtime deprecated do not use, will be removed
 * \return status
 *
 */
status_t PollerThread::init(std::vector<std::shared_ptr<V4L2DeviceBase>> &devices,
                            IPollEventListener *observer,
                            int events,
                            bool makeRealtime)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;

    msg.id = MESSAGE_ID_INIT;
    msg.devices = devices; // copy the vector
    msg.data.init.observer = observer;
    msg.data.init.events = events;
    msg.data.init.makeRealtime = makeRealtime;

    return mMessageQueue.send(&msg, MESSAGE_ID_INIT);
}

status_t PollerThread::handleInit(Message &msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;

    if (mFlushFd[1] != -1 || mFlushFd[0] != -1) {
        close(mFlushFd[0]);
        close(mFlushFd[1]);
        mFlushFd[0] = -1;
        mFlushFd[1] = -1;
    }

    status = pipe(mFlushFd);
    if (status < 0) {
        LOGE("Failed to create Flush pipe: %s", strerror(errno));
        status = NO_INIT;
        goto exitInit;
    }

    /**
     * make the reading end of the pipe non blocking.
     * This helps during flush to read any information left there without
     * blocking
     */
    status = fcntl(mFlushFd[0],F_SETFL,O_NONBLOCK);
    if (status < 0) {
        LOGE("Fail to set flush pipe flag: %s", strerror(errno));
        status = NO_INIT;
        goto exitInit;
    }

    if (msg.data.init.makeRealtime) {
        // Request to change request asynchronously
        LOGW("Real time thread priority change is not supported");
    }

    if (msg.devices.size() == 0) {
        LOGE("%s, No devices provided", __FUNCTION__);
        status = BAD_VALUE;
        goto exitInit;
    }

    if (msg.data.init.observer == nullptr)
    {
        LOGE("%s, No observer provided", __FUNCTION__);
        status = BAD_VALUE;
        goto exitInit;
    }

    mPollingDevices = msg.devices;
    mEvents = msg.data.init.events;

    //attach listener.
    mListener = msg.data.init.observer;
exitInit:
    mMessageQueue.reply(MESSAGE_ID_INIT, status);
    return status;
}

/**
 * pollRequest()
 * this method enqueue the poll request.
 * params: request ID
 *
 */
status_t PollerThread::pollRequest(int reqId, int timeout,
                                   std::vector<std::shared_ptr<V4L2DeviceBase>> *devices)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_POLL_REQUEST;
    msg.data.request.reqId = reqId;
    msg.data.request.timeout = timeout;
    if (devices)
        msg.devices = *devices;

    return mMessageQueue.send(&msg);
}

status_t PollerThread::handlePollRequest(Message &msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = NO_ERROR;
    int ret;
    IPollEventListener::PollEventMessage outMsg;

    if (msg.devices.size() > 0) {
        mPollingDevices = msg.devices;
    } else {
        // Return error for empty poll request
        outMsg.id = IPollEventListener::POLL_EVENT_ID_ERROR;
        outMsg.data.reqId = msg.data.request.reqId;
        outMsg.data.activeDevices = &msg.devices;
        outMsg.data.inactiveDevices = &msg.devices;
        outMsg.data.polledDevices = &msg.devices;
        outMsg.data.pollStatus = 0;

        return notifyListener(&outMsg);
    }

    do {
        PERFORMANCE_ATRACE_NAME("PollRequest");
        ret = V4L2DeviceBase::pollDevices(mPollingDevices, mActiveDevices,
                                          mInactiveDevices,
                                          msg.data.request.timeout, mFlushFd[0],
                                          mEvents);
        if (ret <= 0) {
            outMsg.id = IPollEventListener::POLL_EVENT_ID_ERROR;
        } else {
            outMsg.id = IPollEventListener::POLL_EVENT_ID_EVENT;
        }
        outMsg.data.reqId = msg.data.request.reqId;
        outMsg.data.activeDevices = &mActiveDevices;
        outMsg.data.inactiveDevices = &mInactiveDevices;
        outMsg.data.polledDevices = &mPollingDevices;
        outMsg.data.pollStatus = ret;
        status = notifyListener(&outMsg);
    } while (status == -EAGAIN);
    return status;
}

/**
 * flush()
 * this method is done to interrupt the polling.
 * We first empty the Q for any polling request and then
 * a value is written to a polled fd, which will make the poll returning
 *
 * There are 2 variants an asyncrhonous one that will not wait for the thread
 * to complete the current request and the synchronous one that will send
 * a message to the Q
 *
 * This can be called on an uninitialized Poller also, but the flush will then
 * only empty the message queue and the vectors.
 *
 */
status_t PollerThread::flush(bool sync, bool clear)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_FLUSH;
    msg.data.flush.sync = sync;
    msg.data.flush.clearVectors = clear;

    mMessageQueue.remove(MESSAGE_ID_POLL_REQUEST);

    if (mFlushFd[1] != -1) {
        char buf = 0xf;  // random value to write to flush fd.
        unsigned int size = write(mFlushFd[1], &buf, sizeof(char));
        if (size != sizeof(char))
            LOGW("Flush write not completed");
    }

    if (sync)
        return mMessageQueue.send(&msg, MESSAGE_ID_FLUSH);
    else
        return mMessageQueue.send(&msg);
}

void PollerThread::messageThreadLoop()
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    mThreadRunning = true;

    while (mThreadRunning) {
        status_t status = NO_ERROR;
        Message msg;

        mMessageQueue.receive(&msg);

        PERFORMANCE_HAL_ATRACE_PARAM1("msg", msg.id);
        switch (msg.id) {
        case MESSAGE_ID_EXIT:
            status = handleMessageExit();
            break;

        case MESSAGE_ID_INIT:
            status = handleInit(msg);
            break;

        case MESSAGE_ID_POLL_REQUEST:
            status = handlePollRequest(msg);
            break;

        case MESSAGE_ID_FLUSH:
        {
            /**
             * read the pipe just in case there was nothing to flush.
             * this ensures that the pipe is empty for the next try.
             * this is safe because the reading end is non blocking.
             */
            if (msg.data.flush.clearVectors) {
                mPollingDevices.clear();
                mActiveDevices.clear();
                mInactiveDevices.clear();
            }
            char readbuf;
            if (mFlushFd[0] != -1) {
                unsigned int size = read(mFlushFd[0], (void*) &readbuf, sizeof(char));
                if (size != sizeof(char))
                    LOGW("Flush read not completed.");
            }

            if (msg.data.flush.sync)
                mMessageQueue.reply(MESSAGE_ID_FLUSH, OK);

            break;
        }
        default:
            LOGE("error in handling message: %d, unknown message",
                static_cast<int>(msg.id));
            status = BAD_VALUE;
            break;
        }
        if (status != NO_ERROR)
            LOGE("error %d in handling message: %d", status, (int)msg.id);
    }
}

status_t PollerThread::requestExitAndWait(void)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    Message msg;
    msg.id = MESSAGE_ID_EXIT;
    msg.data.request.reqId = 0;
    status_t status = mMessageQueue.send(&msg, MESSAGE_ID_EXIT);
    status |= mMessageThread->requestExitAndWait();
    return status;
}

status_t PollerThread::handleMessageExit(void)
{
    status_t status = NO_ERROR;
    mThreadRunning = false;
    mMessageQueue.reply(MESSAGE_ID_EXIT, status);
    return status;
}

/** Listener Methods **/
status_t PollerThread::notifyListener(IPollEventListener::PollEventMessage *msg)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    status_t status = OK;

    if (mListener == nullptr)
        return BAD_VALUE;

    status = mListener->notifyPollEvent((IPollEventListener::PollEventMessage*)msg);

    return status;
}

} NAMESPACE_DECLARATION_END

