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
#include <sys/epoll.h>
#include "multihal_sensors.h"

namespace goldfish {

namespace {
int epollCtlAdd(int epollFd, int fd) {
    struct epoll_event ev;
    ev.events  = EPOLLIN;
    ev.data.fd = fd;
    return TEMP_FAILURE_RETRY(epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &ev));
}

int qemuSensortThreadRcvCommand(const int fd) {
    char buf;
    if (TEMP_FAILURE_RETRY(read(fd, &buf, 1)) == 1) {
        return buf;
    } else {
        return -1;
    }
}
}  // namespace

void MultihalSensors::qemuSensorListenerThread() {
    const unique_fd epollFd(epoll_create1(0));
    if (!epollFd.ok()) {
        ALOGE("%s:%d: epoll_create1 failed", __func__, __LINE__);
        ::abort();
    }

    epollCtlAdd(epollFd.get(), m_qemuSensorsFd.get());
    epollCtlAdd(epollFd.get(), m_sensorThreadFd.get());

    QemuSensorsProtocolState protocolState;

    while (true) {
        struct epoll_event events[2];
        const int kTimeoutMs = 60000;
        const int n = TEMP_FAILURE_RETRY(epoll_wait(epollFd.get(),
                                                    events, 2,
                                                    kTimeoutMs));
        if (n < 0) {
            ALOGE("%s:%d: epoll_wait failed with '%s'",
                  __func__, __LINE__, strerror(errno));
            continue;
        }

        for (int i = 0; i < n; ++i) {
            const struct epoll_event* ev = &events[i];
            const int fd = ev->data.fd;
            const int ev_events = ev->events;

            if (fd == m_qemuSensorsFd.get()) {
                if (ev_events & (EPOLLERR | EPOLLHUP)) {
                    ALOGE("%s:%d: epoll_wait: devFd has an error, ev_events=%x",
                          __func__, __LINE__, ev_events);
                    ::abort();
                } else if (ev_events & EPOLLIN) {
                    parseQemuSensorEvent(m_qemuSensorsFd.get(), &protocolState);
                }
            } else if (fd == m_sensorThreadFd.get()) {
                if (ev_events & (EPOLLERR | EPOLLHUP)) {
                    ALOGE("%s:%d: epoll_wait: threadsFd has an error, ev_events=%x",
                          __func__, __LINE__, ev_events);
                    ::abort();
                } else if (ev_events & EPOLLIN) {
                    const int cmd = qemuSensortThreadRcvCommand(fd);
                    if (cmd == kCMD_QUIT) {
                        return;
                    } else {
                        ALOGE("%s:%d: qemuSensortThreadRcvCommand returned unexpected command, cmd=%d",
                              __func__, __LINE__, cmd);
                        ::abort();
                    }
                }
            } else {
                ALOGE("%s:%d: epoll_wait() returned unexpected fd",
                      __func__, __LINE__);
            }
        }
    }
}

}  // namespace goldfish
