/*
 *
 * Copyright 2018, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <secure_input/evdev.h>

#include <android-base/logging.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <teeui/msg_formatting.h>
#include <time.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>

namespace secure_input {

bool EventLoop::timerOrder(const Timer& a, const Timer& b) {
    return a.next > b.next;
}

void EventLoop::processNewTimers() {
    for (auto& timer : newTimers_) {
        timers_.push_back(std::move(timer));
        std::push_heap(timers_.begin(), timers_.end(), timerOrder);
    }
    newTimers_.clear();
}

int EventLoop::runTimers() {
    using namespace std::chrono_literals;
    auto now = std::chrono::steady_clock::now();
    while (!timers_.empty() && timers_[0].next <= now) {
        std::pop_heap(timers_.begin(), timers_.end(), timerOrder);
        auto& current = *timers_.rbegin();
        current.handleTimer();
        if (!current.oneShot) {
            auto diff = now - current.next;
            current.next += ((diff / current.duration) + 1) * current.duration;
            std::push_heap(timers_.begin(), timers_.end(), timerOrder);
        } else {
            timers_.pop_back();
        }
    }
    if (timers_.empty()) return -1;
    auto& next = *timers_.begin();
    auto diff = next.next - now;
    if (diff > 60s) {
        return 60000;
    } else {
        return std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
    }
}

void EventLoop::processNewReceivers() {
    for (auto& receiver : newReceivers_) {
        receivers_.push_back(std::move(receiver));
    }
    newReceivers_.clear();
}

void EventLoop::addEventReceiver(NonCopyableFunction<void(short)> handler, int eventFd,
                                 short flags) {
    std::unique_lock<std::mutex> lock(mutex_);
    newReceivers_.emplace_back(eventFd, flags, std::move(handler));
    if (eventFd_ != -1) {
        eventfd_write(eventFd_, 1);
    }
}

void EventLoop::addTimer(NonCopyableFunction<void()> handler,
                         std::chrono::steady_clock::duration duration, bool oneShot) {
    std::unique_lock<std::mutex> lock(mutex_);
    std::chrono::steady_clock::time_point next = std::chrono::steady_clock::now() + duration;
    newTimers_.emplace_back(next, duration, std::move(handler), oneShot);
    if (eventFd_ != -1) {
        eventfd_write(eventFd_, 1);
    }
}

bool EventLoop::start() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (state_ != ThreadState::JOINED) return false;
    eventFd_ = eventfd(0, EFD_CLOEXEC);
    if (eventFd_ == -1) return false;
    state_ = ThreadState::STARTING;

    thread_ = std::thread([this]() {
        std::unique_lock<std::mutex> lock(mutex_);
        state_ = ThreadState::RUNNING;
        lock.unlock();
        condVar_.notify_all();
        lock.lock();
        while (state_ == ThreadState::RUNNING) {
            processNewTimers();
            processNewReceivers();  // must be called while locked
            lock.unlock();
            std::vector<pollfd> fds(receivers_.size() + 1);
            fds[0] = {eventFd_, POLLIN, 0};
            unsigned i = 1;
            for (auto& receiver : receivers_) {
                fds[i] = {receiver.eventFd, receiver.eventFlags, 0};
                ++i;
            }
            auto rc = poll(fds.data(), fds.size(), runTimers());
            if (state_ != ThreadState::RUNNING) {
                lock.lock();
                break;
            }
            if (rc > 0) {
                // don't handle eventFd_ explicitly
                i = 1;
                for (auto& receiver : receivers_) {
                    if (fds[i].revents & receiver.eventFlags) {
                        receiver.handleEvent(fds[i].revents);
                    }
                    ++i;
                }
            } else if (rc < 0) {
                LOG(ERROR) << __func__ << " poll failed with errno: " << strerror(errno);
            }
            lock.lock();
        }
        state_ = ThreadState::TERMINATING;
        lock.unlock();
        condVar_.notify_all();
    });
    condVar_.wait(lock, [this]() -> bool { return state_ != ThreadState::STARTING; });
    return state_ == ThreadState::RUNNING;
}

void EventLoop::stop() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (state_ == ThreadState::JOINED) return;
    if (state_ == ThreadState::RUNNING) {
        state_ = ThreadState::STOP_REQUESTED;
        eventfd_write(eventFd_, 1);
    }
    condVar_.wait(lock, [this]() -> bool { return state_ == ThreadState::TERMINATING; });
    thread_.join();
    close(eventFd_);
    state_ = ThreadState::JOINED;
    LOG(DEBUG) << "Done ending event polling";
}

EventLoop::~EventLoop() {
    stop();
}

EventDev::EventDev() : fd_(-1), path_("") {}
EventDev::EventDev(const std::string& path) : fd_(-1), path_(path) {}
EventDev::EventDev(EventDev&& other) : fd_(other.fd_), path_(std::move(other.path_)) {
    other.fd_ = -1;
}
EventDev& EventDev::operator=(EventDev&& other) {
    if (&other == this) return *this;
    fd_ = other.fd_;
    path_ = std::move(other.path_);
    other.fd_ = -1;
    return *this;
}
bool EventDev::grab() {
    if (fd_ >= 0) {
        return true;
    }
    fd_ = TEMP_FAILURE_RETRY(open(path_.c_str(), O_RDWR | O_NONBLOCK));
    if (fd_ < 0) {
        LOG(ERROR) << "failed to open event device \"" << path_ << "\"";
        return false;
    }
    int error = ioctl(fd_, EVIOCGRAB, 1);
    if (error) {
        LOG(ERROR) << "failed to grab event device " << path_ << " exclusively EVIOCGRAB returned "
                   << error << " " << strerror(errno);
        close(fd_);
        fd_ = -1;
        return false;
    }
    return true;
}

void EventDev::ungrab() {
    if (fd_ < 0) {
        return;
    }
    int error = ioctl(fd_, EVIOCGRAB, 0);
    if (error) {
        LOG(ERROR) << "failed to ungrab \"" << path_ << "\" EVIOCGRAB returned " << error;
    }
    close(fd_);
    fd_ = -1;
}

std::tuple<bool, input_event> EventDev::readEvent() const {
    std::tuple<bool, input_event> result{false, {}};
    ssize_t rc;
    rc = TEMP_FAILURE_RETRY(read(fd_, &std::get<1>(result), sizeof std::get<1>(result)));
    std::get<0>(result) = rc == sizeof std::get<1>(result);
    return result;
}

int EventDev::fd() const {
    return fd_;
}

bool grabAllEvDevsAndRegisterCallbacks(EventLoop* eventloop,
                                       std::function<void(short, const EventDev&)> handler) {
    if (!eventloop) return false;
    dirent** dirs = nullptr;
    int n = scandir(
        "/dev/input", &dirs,
        [](const dirent* dir) -> int {
            return (dir->d_type & DT_CHR) && !strncmp("event", dir->d_name, 5);
        },
        alphasort);
    if (n < 0) {
        LOG(WARNING) << "Unable to enumerate input devices " << strerror(errno);
        return true;
    }

    bool result = true;
    for (int i = 0; i < n; ++i) {
        EventDev evDev(std::string("/dev/input/") + dirs[i]->d_name);
        result = result && evDev.grab();
        int fd = evDev.fd();
        eventloop->addEventReceiver(
            [&, handler, evDev = std::move(evDev)](short flags) { handler(flags, evDev); }, fd,
            POLLIN);
        free(dirs[i]);
    }
    free(dirs);
    // true if all devices where grabbed successfully
    return result;
}

}  // namespace secure_input
