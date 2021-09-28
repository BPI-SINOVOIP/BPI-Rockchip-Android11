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

#ifndef CONFIRMATIONUI_EVDEV_H_
#define CONFIRMATIONUI_EVDEV_H_

#include <linux/input.h>
#include <poll.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

namespace secure_input {

template <typename Fn> class NonCopyableFunction;

template <typename Ret, typename... Args> class NonCopyableFunction<Ret(Args...)> {
    class NonCopyableFunctionBase {
      public:
        NonCopyableFunctionBase() = default;
        virtual ~NonCopyableFunctionBase() {}
        virtual Ret operator()(Args... args) = 0;
        NonCopyableFunctionBase(const NonCopyableFunctionBase&) = delete;
        NonCopyableFunctionBase& operator=(const NonCopyableFunctionBase&) = delete;
    };

    template <typename Fn> class NonCopyableFunctionTypeEraser : public NonCopyableFunctionBase {
      private:
        Fn f_;

      public:
        NonCopyableFunctionTypeEraser() = default;
        explicit NonCopyableFunctionTypeEraser(Fn f) : f_(std::move(f)) {}
        Ret operator()(Args... args) override { return f_(std::move(args)...); }
    };

  private:
    std::unique_ptr<NonCopyableFunctionBase> f_;

  public:
    NonCopyableFunction() = default;
    template <typename F> NonCopyableFunction(F f) {
        f_ = std::make_unique<NonCopyableFunctionTypeEraser<F>>(std::move(f));
    }
    NonCopyableFunction(NonCopyableFunction&& other) = default;
    NonCopyableFunction& operator=(NonCopyableFunction&& other) = default;
    NonCopyableFunction(const NonCopyableFunction& other) = delete;
    NonCopyableFunction& operator=(const NonCopyableFunction& other) = delete;

    Ret operator()(Args... args) {
        if (f_) return (*f_)(std::move(args)...);
    }
};

class EventLoop {
  private:
    enum class ThreadState : long {
        STARTING,
        RUNNING,
        STOP_REQUESTED,
        JOINED,
        TERMINATING,
    };
    std::thread thread_;
    int eventFd_ = -1;
    volatile ThreadState state_ = ThreadState::JOINED;
    std::mutex mutex_;
    std::condition_variable condVar_;

    struct EventReceiver {
        EventReceiver(int fd, short flags, NonCopyableFunction<void(short)> handle)
            : eventFd(fd), eventFlags(flags), handleEvent(std::move(handle)) {}
        int eventFd;
        short eventFlags;
        NonCopyableFunction<void(short)> handleEvent;
    };

    std::vector<EventReceiver> receivers_;
    std::list<EventReceiver> newReceivers_;

    struct Timer {
        Timer(std::chrono::steady_clock::time_point _next,
              std::chrono::steady_clock::duration _duration, NonCopyableFunction<void()> handle,
              bool _oneShot)
            : next(_next), duration(_duration), handleTimer(std::move(handle)), oneShot(_oneShot) {}

        std::chrono::steady_clock::time_point next;
        std::chrono::steady_clock::duration duration;
        NonCopyableFunction<void()> handleTimer;
        bool oneShot;
    };
    std::vector<Timer> timers_;
    std::list<Timer> newTimers_;

    static bool timerOrder(const Timer& a, const Timer& b);

    void processNewTimers();

    int runTimers();
    void processNewReceivers();

  public:
    ~EventLoop();
    void addEventReceiver(NonCopyableFunction<void(short)> handler, int eventFd,
                          short flags = POLLIN);

    void addTimer(NonCopyableFunction<void()> handler, std::chrono::steady_clock::duration duration,
                  bool oneShot = true);

    bool start();
    void stop();
};

class EventDev {
  private:
    int fd_ = -1;
    std::string path_;

  public:
    EventDev();
    EventDev(const std::string& path);
    EventDev(const EventDev&) = delete;
    EventDev(EventDev&& other);
    EventDev& operator=(EventDev&& other);
    ~EventDev() { ungrab(); }
    bool grab();
    void ungrab();
    std::tuple<bool, input_event> readEvent() const;
    int fd() const;
};

bool grabAllEvDevsAndRegisterCallbacks(EventLoop* eventloop,
                                       std::function<void(short, const EventDev&)> handler);

}  // namespace secure_input

#endif  // CONFIRMATIONUI_EVDEV_H_
