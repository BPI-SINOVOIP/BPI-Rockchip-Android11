/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <log/log.h>
#include <fcntl.h>
#include <qemud.h>
#include <qemu_pipe_bp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include "gnss_hw_conn.h"
#include "gnss_hw_listener.h"

namespace {
constexpr char kCMD_QUIT = 'q';
constexpr char kCMD_START = 'a';
constexpr char kCMD_STOP = 'o';

int epollCtlAdd(int epollFd, int fd) {
    int ret;

    /* make the fd non-blocking */
    ret = TEMP_FAILURE_RETRY(fcntl(fd, F_GETFL));
    if (ret < 0) {
        return ret;
    }
    ret = TEMP_FAILURE_RETRY(fcntl(fd, F_SETFL, ret | O_NONBLOCK));
    if (ret < 0) {
        return ret;
    }

    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = fd;

    return TEMP_FAILURE_RETRY(epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev));
}
}  // namespace

namespace goldfish {

GnssHwConn::GnssHwConn(const DataSink* sink) {
    m_devFd.reset(qemu_pipe_open_ns("qemud", "gps", O_RDWR));
    if (!m_devFd.ok()) {
        ALOGE("%s:%d: qemu_pipe_open_ns failed", __PRETTY_FUNCTION__, __LINE__);
        return;
    }

    if (!::android::base::Socketpair(AF_LOCAL, SOCK_STREAM, 0,
                                     &m_callersFd, &m_threadsFd)) {
        ALOGE("%s:%d: Socketpair failed", __PRETTY_FUNCTION__, __LINE__);
        m_devFd.reset();
        return;
    }

    m_thread = std::thread([this, sink]() {
        sink->gnssStatus(ahg10::IGnssCallback::GnssStatusValue::ENGINE_ON);
        workerThread(m_devFd.get(), m_threadsFd.get(), sink);
        sink->gnssStatus(ahg10::IGnssCallback::GnssStatusValue::ENGINE_OFF);
    });
}

GnssHwConn::~GnssHwConn() {
    if (m_thread.joinable()) {
        sendWorkerThreadCommand(kCMD_QUIT);
        m_thread.join();
    }
}

bool GnssHwConn::ok() const {
    return m_thread.joinable();
}

bool GnssHwConn::start() {
    return ok() && sendWorkerThreadCommand(kCMD_START);
}

bool GnssHwConn::stop() {
    return ok() && sendWorkerThreadCommand(kCMD_STOP);
}

void GnssHwConn::workerThread(int devFd, int threadsFd, const DataSink* sink) {
    const unique_fd epollFd(epoll_create1(0));
    if (!epollFd.ok()) {
        ALOGE("%s:%d: epoll_create1 failed", __PRETTY_FUNCTION__, __LINE__);
        ::abort();
    }

    epollCtlAdd(epollFd.get(), devFd);
    epollCtlAdd(epollFd.get(), threadsFd);

    GnssHwListener listener(sink);
    bool running = false;

    while (true) {
        struct epoll_event events[2];
        const int kTimeoutMs = 60000;
        const int n = TEMP_FAILURE_RETRY(epoll_wait(epollFd.get(),
                                                    events, 2,
                                                    kTimeoutMs));
        if (n < 0) {
            ALOGE("%s:%d: epoll_wait failed with '%s'",
                  __PRETTY_FUNCTION__, __LINE__, strerror(errno));
            continue;
        }

        for (int i = 0; i < n; ++i) {
            const struct epoll_event* ev = &events[i];
            const int fd = ev->data.fd;
            const int ev_events = ev->events;

            if (fd == devFd) {
                if (ev_events & (EPOLLERR | EPOLLHUP)) {
                    ALOGE("%s:%d: epoll_wait: devFd has an error, ev_events=%x",
                          __PRETTY_FUNCTION__, __LINE__, ev_events);
                    ::abort();
                } else if (ev_events & EPOLLIN) {
                    char buf[64];
                    while (true) {
                        int n = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
                        if (n > 0) {
                            if (running) {
                                for (int i = 0; i < n; ++i) {
                                    listener.consume(buf[i]);
                                }
                            }
                        } else {
                            break;
                        }
                    }
                }
            } else if (fd == threadsFd) {
                if (ev_events & (EPOLLERR | EPOLLHUP)) {
                    ALOGE("%s:%d: epoll_wait: threadsFd has an error, ev_events=%x",
                          __PRETTY_FUNCTION__, __LINE__, ev_events);
                    ::abort();
                } else if (ev_events & EPOLLIN) {
                    const int cmd = workerThreadRcvCommand(fd);
                    switch (cmd) {
                        case kCMD_QUIT:
                            return;

                        case kCMD_START:
                            if (!running) {
                                listener.reset();
                                sink->gnssStatus(ahg10::IGnssCallback::GnssStatusValue::SESSION_BEGIN);
                                running = true;
                            }
                            break;

                        case kCMD_STOP:
                            if (running) {
                                running = false;
                                sink->gnssStatus(ahg10::IGnssCallback::GnssStatusValue::SESSION_END);
                            }
                            break;

                        default:
                            ALOGE("%s:%d: workerThreadRcvCommand returned unexpected command, cmd=%d",
                                  __PRETTY_FUNCTION__, __LINE__, cmd);
                            ::abort();
                            break;
                    }
                }
            } else {
                ALOGE("%s:%d: epoll_wait() returned unexpected fd",
                      __PRETTY_FUNCTION__, __LINE__);
            }
        }
    }
}

int GnssHwConn::workerThreadRcvCommand(const int fd) {
    char buf;
    if (TEMP_FAILURE_RETRY(read(fd, &buf, 1)) == 1) {
        return buf;
    } else {
        return -1;
    }
}

bool GnssHwConn::sendWorkerThreadCommand(char cmd) const {
    return TEMP_FAILURE_RETRY(write(m_callersFd.get(), &cmd, 1)) == 1;
}

}  // namespace goldfish
