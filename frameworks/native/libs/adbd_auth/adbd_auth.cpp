/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define ANDROID_BASE_UNIQUE_FD_DISABLE_IMPLICIT_CONVERSION

#include "include/adbd_auth.h"

#include <inttypes.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/uio.h>

#include <chrono>
#include <deque>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/strings.h>
#include <android-base/thread_annotations.h>
#include <android-base/unique_fd.h>
#include <cutils/sockets.h>

using android::base::unique_fd;

static constexpr uint32_t kAuthVersion = 1;

struct AdbdAuthPacketAuthenticated {
    std::string public_key;
};

struct AdbdAuthPacketDisconnected {
    std::string public_key;
};

struct AdbdAuthPacketRequestAuthorization {
    std::string public_key;
};

struct AdbdPacketTlsDeviceConnected {
    uint8_t transport_type;
    std::string public_key;
};

struct AdbdPacketTlsDeviceDisconnected {
    uint8_t transport_type;
    std::string public_key;
};

using AdbdAuthPacket = std::variant<AdbdAuthPacketAuthenticated,
                                    AdbdAuthPacketDisconnected,
                                    AdbdAuthPacketRequestAuthorization,
                                    AdbdPacketTlsDeviceConnected,
                                    AdbdPacketTlsDeviceDisconnected>;

struct AdbdAuthContext {
    static constexpr uint64_t kEpollConstSocket = 0;
    static constexpr uint64_t kEpollConstEventFd = 1;
    static constexpr uint64_t kEpollConstFramework = 2;

public:
    explicit AdbdAuthContext(AdbdAuthCallbacksV1* callbacks) : next_id_(0), callbacks_(*callbacks) {
        InitFrameworkHandlers();
        epoll_fd_.reset(epoll_create1(EPOLL_CLOEXEC));
        if (epoll_fd_ == -1) {
            PLOG(FATAL) << "adbd_auth: failed to create epoll fd";
        }

        event_fd_.reset(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK));
        if (event_fd_ == -1) {
            PLOG(FATAL) << "adbd_auth: failed to create eventfd";
        }

        sock_fd_.reset(android_get_control_socket("adbd"));
        if (sock_fd_ == -1) {
            PLOG(ERROR) << "adbd_auth: failed to get adbd authentication socket";
        } else {
            if (fcntl(sock_fd_.get(), F_SETFD, FD_CLOEXEC) != 0) {
                PLOG(FATAL) << "adbd_auth: failed to make adbd authentication socket cloexec";
            }

            if (fcntl(sock_fd_.get(), F_SETFL, O_NONBLOCK) != 0) {
                PLOG(FATAL) << "adbd_auth: failed to make adbd authentication socket nonblocking";
            }

            if (listen(sock_fd_.get(), 4) != 0) {
                PLOG(FATAL) << "adbd_auth: failed to listen on adbd authentication socket";
            }
        }
    }

    AdbdAuthContext(const AdbdAuthContext& copy) = delete;
    AdbdAuthContext(AdbdAuthContext&& move) = delete;
    AdbdAuthContext& operator=(const AdbdAuthContext& copy) = delete;
    AdbdAuthContext& operator=(AdbdAuthContext&& move) = delete;

    uint64_t NextId() { return next_id_++; }

    void DispatchPendingPrompt() REQUIRES(mutex_) {
        if (dispatched_prompt_) {
            LOG(INFO) << "adbd_auth: prompt currently pending, skipping";
            return;
        }

        if (pending_prompts_.empty()) {
            LOG(INFO) << "adbd_auth: no prompts to send";
            return;
        }

        LOG(INFO) << "adbd_auth: prompting user for adb authentication";
        auto [id, public_key, arg] = std::move(pending_prompts_.front());
        pending_prompts_.pop_front();

        this->output_queue_.emplace_back(
                AdbdAuthPacketRequestAuthorization{.public_key = public_key});

        Interrupt();
        dispatched_prompt_ = std::make_tuple(id, public_key, arg);
    }

    void UpdateFrameworkWritable() REQUIRES(mutex_) {
        // This might result in redundant calls to EPOLL_CTL_MOD if, for example, we get notified
        // at the same time as a framework connection, but that's unlikely and this doesn't need to
        // be fast anyway.
        if (framework_fd_ != -1) {
            struct epoll_event event;
            event.events = EPOLLIN;
            if (!output_queue_.empty()) {
                LOG(INFO) << "adbd_auth: marking framework writable";
                event.events |= EPOLLOUT;
            }
            event.data.u64 = kEpollConstFramework;
            CHECK_EQ(0, epoll_ctl(epoll_fd_.get(), EPOLL_CTL_MOD, framework_fd_.get(), &event));
        }
    }

    void ReplaceFrameworkFd(unique_fd new_fd) REQUIRES(mutex_) {
        LOG(INFO) << "adbd_auth: received new framework fd " << new_fd.get()
                  << " (current = " << framework_fd_.get() << ")";

        // If we already had a framework fd, clean up after ourselves.
        if (framework_fd_ != -1) {
            output_queue_.clear();
            dispatched_prompt_.reset();
            CHECK_EQ(0, epoll_ctl(epoll_fd_.get(), EPOLL_CTL_DEL, framework_fd_.get(), nullptr));
            framework_fd_.reset();
        }

        if (new_fd != -1) {
            struct epoll_event event;
            event.events = EPOLLIN;
            if (!output_queue_.empty()) {
                LOG(INFO) << "adbd_auth: marking framework writable";
                event.events |= EPOLLOUT;
            }
            event.data.u64 = kEpollConstFramework;
            CHECK_EQ(0, epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, new_fd.get(), &event));
            framework_fd_ = std::move(new_fd);
        }
    }

    void HandlePacket(std::string_view packet) EXCLUDES(mutex_) {
        LOG(INFO) << "adbd_auth: received packet: " << packet;

        if (packet.size() < 2) {
            LOG(ERROR) << "adbd_auth: received packet of invalid length";
            std::lock_guard<std::mutex> lock(mutex_);
            ReplaceFrameworkFd(unique_fd());
        }

        bool handled_packet = false;
        for (size_t i = 0; i < framework_handlers_.size(); ++i) {
            if (android::base::ConsumePrefix(&packet, framework_handlers_[i].code)) {
                framework_handlers_[i].cb(packet);
                handled_packet = true;
                break;
            }
        }
        if (!handled_packet) {
            LOG(ERROR) << "adbd_auth: unhandled packet: " << packet;
            std::lock_guard<std::mutex> lock(mutex_);
            ReplaceFrameworkFd(unique_fd());
        }
    }

    void AllowUsbDevice(std::string_view buf) EXCLUDES(mutex_) {
        std::lock_guard<std::mutex> lock(mutex_);
        CHECK(buf.empty());

        if (dispatched_prompt_.has_value()) {
            // It's possible for the framework to send us a response without our having sent a
            // request to it: e.g. if adbd restarts while we have a pending request.
            auto& [id, key, arg] = *dispatched_prompt_;
            keys_.emplace(id, std::move(key));

            callbacks_.key_authorized(arg, id);
            dispatched_prompt_ = std::nullopt;
        } else {
            LOG(WARNING) << "adbd_auth: received authorization for unknown prompt, ignoring";
        }

        // We need to dispatch pending prompts here upon success as well,
        // since we might have multiple queued prompts.
        DispatchPendingPrompt();
    }

    void DenyUsbDevice(std::string_view buf) EXCLUDES(mutex_) {
        std::lock_guard<std::mutex> lock(mutex_);
        CHECK(buf.empty());
        // TODO: Do we want a callback if the key is denied?
        dispatched_prompt_ = std::nullopt;
        DispatchPendingPrompt();
    }

    void KeyRemoved(std::string_view buf) EXCLUDES(mutex_) {
        CHECK(!buf.empty());
        callbacks_.key_removed(buf.data(), buf.size());
    }

    bool SendPacket() REQUIRES(mutex_) {
        if (output_queue_.empty()) {
            return false;
        }

        CHECK_NE(-1, framework_fd_.get());

        auto& packet = output_queue_.front();
        struct iovec iovs[3];
        int iovcnt = 2;
        if (auto* p = std::get_if<AdbdAuthPacketAuthenticated>(&packet)) {
            iovs[0].iov_base = const_cast<char*>("CK");
            iovs[0].iov_len = 2;
            iovs[1].iov_base = p->public_key.data();
            iovs[1].iov_len = p->public_key.size();
        } else if (auto* p = std::get_if<AdbdAuthPacketDisconnected>(&packet)) {
            iovs[0].iov_base = const_cast<char*>("DC");
            iovs[0].iov_len = 2;
            iovs[1].iov_base = p->public_key.data();
            iovs[1].iov_len = p->public_key.size();
        } else if (auto* p = std::get_if<AdbdAuthPacketRequestAuthorization>(&packet)) {
            iovs[0].iov_base = const_cast<char*>("PK");
            iovs[0].iov_len = 2;
            iovs[1].iov_base = p->public_key.data();
            iovs[1].iov_len = p->public_key.size();
        } else if (auto* p = std::get_if<AdbdPacketTlsDeviceConnected>(&packet)) {
            iovcnt = 3;
            iovs[0].iov_base = const_cast<char*>("WE");
            iovs[0].iov_len = 2;
            iovs[1].iov_base = &p->transport_type;
            iovs[1].iov_len = 1;
            iovs[2].iov_base = p->public_key.data();
            iovs[2].iov_len = p->public_key.size();
        } else if (auto* p = std::get_if<AdbdPacketTlsDeviceDisconnected>(&packet)) {
            iovcnt = 3;
            iovs[0].iov_base = const_cast<char*>("WF");
            iovs[0].iov_len = 2;
            iovs[1].iov_base = &p->transport_type;
            iovs[1].iov_len = 1;
            iovs[2].iov_base = p->public_key.data();
            iovs[2].iov_len = p->public_key.size();
        } else {
            LOG(FATAL) << "adbd_auth: unhandled packet type?";
        }

        output_queue_.pop_front();

        ssize_t rc = writev(framework_fd_.get(), iovs, iovcnt);
        if (rc == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            PLOG(ERROR) << "adbd_auth: failed to write to framework fd";
            ReplaceFrameworkFd(unique_fd());
            return false;
        }

        return true;
    }

    void Run() {
        if (sock_fd_ == -1) {
            LOG(ERROR) << "adbd_auth: socket unavailable, disabling user prompts";
        } else {
            struct epoll_event event;
            event.events = EPOLLIN;
            event.data.u64 = kEpollConstSocket;
            CHECK_EQ(0, epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, sock_fd_.get(), &event));
        }

        {
            struct epoll_event event;
            event.events = EPOLLIN;
            event.data.u64 = kEpollConstEventFd;
            CHECK_EQ(0, epoll_ctl(epoll_fd_.get(), EPOLL_CTL_ADD, event_fd_.get(), &event));
        }

        while (true) {
            struct epoll_event events[3];
            int rc = TEMP_FAILURE_RETRY(epoll_wait(epoll_fd_.get(), events, 3, -1));
            if (rc == -1) {
                PLOG(FATAL) << "adbd_auth: epoll_wait failed";
            } else if (rc == 0) {
                LOG(FATAL) << "adbd_auth: epoll_wait returned 0";
            }

            bool restart = false;
            for (int i = 0; i < rc; ++i) {
                if (restart) {
                    break;
                }

                struct epoll_event& event = events[i];
                switch (event.data.u64) {
                    case kEpollConstSocket: {
                        unique_fd new_framework_fd(accept4(sock_fd_.get(), nullptr, nullptr,
                                                           SOCK_CLOEXEC | SOCK_NONBLOCK));
                        if (new_framework_fd == -1) {
                            PLOG(FATAL) << "adbd_auth: failed to accept framework fd";
                        }

                        LOG(INFO) << "adbd_auth: received a new framework connection";
                        std::lock_guard<std::mutex> lock(mutex_);
                        ReplaceFrameworkFd(std::move(new_framework_fd));

                        // Stop iterating over events: one of the later ones might be the old
                        // framework fd.
                        restart = false;
                        break;
                    }

                    case kEpollConstEventFd: {
                        // We were woken up to write something.
                        uint64_t dummy;
                        int rc = TEMP_FAILURE_RETRY(read(event_fd_.get(), &dummy, sizeof(dummy)));
                        if (rc != 8) {
                            PLOG(FATAL)
                                    << "adbd_auth: failed to read from eventfd (rc = " << rc << ")";
                        }

                        std::lock_guard<std::mutex> lock(mutex_);
                        UpdateFrameworkWritable();
                        break;
                    }

                    case kEpollConstFramework: {
                        char buf[4096];
                        if (event.events & EPOLLIN) {
                            int rc = TEMP_FAILURE_RETRY(read(framework_fd_.get(), buf, sizeof(buf)));
                            if (rc == -1) {
                                LOG(FATAL) << "adbd_auth: failed to read from framework fd";
                            } else if (rc == 0) {
                                LOG(INFO) << "adbd_auth: hit EOF on framework fd";
                                std::lock_guard<std::mutex> lock(mutex_);
                                ReplaceFrameworkFd(unique_fd());
                            } else {
                                HandlePacket(std::string_view(buf, rc));
                            }
                        }

                        if (event.events & EPOLLOUT) {
                            std::lock_guard<std::mutex> lock(mutex_);
                            while (SendPacket()) {
                                continue;
                            }
                            UpdateFrameworkWritable();
                        }

                        break;
                    }
                }
            }
        }
    }

    static constexpr const char* key_paths[] = {"/adb_keys", "/data/misc/adb/adb_keys"};
    void IteratePublicKeys(bool (*callback)(void*, const char*, size_t), void* opaque) {
        for (const auto& path : key_paths) {
            if (access(path, R_OK) == 0) {
                LOG(INFO) << "adbd_auth: loading keys from " << path;
                std::string content;
                if (!android::base::ReadFileToString(path, &content)) {
                    PLOG(ERROR) << "adbd_auth: couldn't read " << path;
                    continue;
                }
                for (const auto& line : android::base::Split(content, "\n")) {
                    if (!callback(opaque, line.data(), line.size())) {
                        return;
                    }
                }
            }
        }
    }

    uint64_t PromptUser(std::string_view public_key, void* arg) EXCLUDES(mutex_) {
        uint64_t id = NextId();

        std::lock_guard<std::mutex> lock(mutex_);
        LOG(INFO) << "adbd_auth: sending prompt with id " << id;
        pending_prompts_.emplace_back(id, public_key, arg);
        DispatchPendingPrompt();
        return id;
    }

    uint64_t NotifyAuthenticated(std::string_view public_key) EXCLUDES(mutex_) {
        uint64_t id = NextId();
        std::lock_guard<std::mutex> lock(mutex_);
        keys_.emplace(id, public_key);
        output_queue_.emplace_back(
                AdbdAuthPacketAuthenticated{.public_key = std::string(public_key)});
        return id;
    }

    void NotifyDisconnected(uint64_t id) EXCLUDES(mutex_) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = keys_.find(id);
        if (it == keys_.end()) {
            LOG(DEBUG) << "adbd_auth: couldn't find public key to notify disconnection, skipping";
            return;
        }
        output_queue_.emplace_back(AdbdAuthPacketDisconnected{.public_key = std::move(it->second)});
        keys_.erase(it);
    }

    uint64_t NotifyTlsDeviceConnected(AdbTransportType type,
                                      std::string_view public_key) EXCLUDES(mutex_) {
        uint64_t id = NextId();
        std::lock_guard<std::mutex> lock(mutex_);
        keys_.emplace(id, public_key);
        output_queue_.emplace_back(AdbdPacketTlsDeviceConnected{
                .transport_type = static_cast<uint8_t>(type),
                .public_key = std::string(public_key)});
        Interrupt();
        return id;
    }

    void NotifyTlsDeviceDisconnected(AdbTransportType type, uint64_t id) EXCLUDES(mutex_) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = keys_.find(id);
        if (it == keys_.end()) {
            LOG(DEBUG) << "adbd_auth: couldn't find public key to notify disconnection of tls "
                          "device, skipping";
            return;
        }
        output_queue_.emplace_back(AdbdPacketTlsDeviceDisconnected{
                .transport_type = static_cast<uint8_t>(type),
                .public_key = std::move(it->second)});
        keys_.erase(it);
        Interrupt();
    }

    // Interrupt the worker thread to do some work.
    void Interrupt() {
        uint64_t value = 1;
        ssize_t rc = write(event_fd_.get(), &value, sizeof(value));
        if (rc == -1) {
            PLOG(FATAL) << "adbd_auth: write to eventfd failed";
        } else if (rc != sizeof(value)) {
            LOG(FATAL) << "adbd_auth: write to eventfd returned short (" << rc << ")";
        }
    }

    void InitFrameworkHandlers() {
        // Framework wants to disconnect from a secured wifi device
        framework_handlers_.emplace_back(
                FrameworkPktHandler{
                    .code = "DD",
                    .cb = std::bind(&AdbdAuthContext::KeyRemoved, this, std::placeholders::_1)});
        // Framework allows USB debugging for the device
        framework_handlers_.emplace_back(
                FrameworkPktHandler{
                    .code = "OK",
                    .cb = std::bind(&AdbdAuthContext::AllowUsbDevice, this, std::placeholders::_1)});
        // Framework denies USB debugging for the device
        framework_handlers_.emplace_back(
                FrameworkPktHandler{
                    .code = "NO",
                    .cb = std::bind(&AdbdAuthContext::DenyUsbDevice, this, std::placeholders::_1)});
    }

    unique_fd epoll_fd_;
    unique_fd event_fd_;
    unique_fd sock_fd_;
    unique_fd framework_fd_;

    std::atomic<uint64_t> next_id_;
    AdbdAuthCallbacksV1 callbacks_;

    std::mutex mutex_;
    std::unordered_map<uint64_t, std::string> keys_ GUARDED_BY(mutex_);

    // We keep two separate queues: one to handle backpressure from the socket (output_queue_)
    // and one to make sure we only dispatch one authrequest at a time (pending_prompts_).
    std::deque<AdbdAuthPacket> output_queue_ GUARDED_BY(mutex_);

    std::optional<std::tuple<uint64_t, std::string, void*>> dispatched_prompt_ GUARDED_BY(mutex_);
    std::deque<std::tuple<uint64_t, std::string, void*>> pending_prompts_ GUARDED_BY(mutex_);

    // This is a list of commands that the framework could send to us.
    using FrameworkHandlerCb = std::function<void(std::string_view)>;
    struct FrameworkPktHandler {
        const char* code;
        FrameworkHandlerCb cb;
    };
    std::vector<FrameworkPktHandler> framework_handlers_;
};

AdbdAuthContext* adbd_auth_new(AdbdAuthCallbacks* callbacks) {
    if (callbacks->version == 1) {
        return new AdbdAuthContext(reinterpret_cast<AdbdAuthCallbacksV1*>(callbacks));
    } else {
        LOG(ERROR) << "adbd_auth: received unknown AdbdAuthCallbacks version "
                   << callbacks->version;
        return nullptr;
    }
}

void adbd_auth_delete(AdbdAuthContext* ctx) {
    delete ctx;
}

void adbd_auth_run(AdbdAuthContext* ctx) {
    return ctx->Run();
}

void adbd_auth_get_public_keys(AdbdAuthContext* ctx,
                               bool (*callback)(void* opaque, const char* public_key, size_t len),
                               void* opaque) {
    ctx->IteratePublicKeys(callback, opaque);
}

uint64_t adbd_auth_notify_auth(AdbdAuthContext* ctx, const char* public_key, size_t len) {
    return ctx->NotifyAuthenticated(std::string_view(public_key, len));
}

void adbd_auth_notify_disconnect(AdbdAuthContext* ctx, uint64_t id) {
    return ctx->NotifyDisconnected(id);
}

void adbd_auth_prompt_user(AdbdAuthContext* ctx, const char* public_key, size_t len,
                           void* opaque) {
    adbd_auth_prompt_user_with_id(ctx, public_key, len, opaque);
}

uint64_t adbd_auth_prompt_user_with_id(AdbdAuthContext* ctx, const char* public_key, size_t len,
                                       void* opaque) {
    return ctx->PromptUser(std::string_view(public_key, len), opaque);
}

uint64_t adbd_auth_tls_device_connected(AdbdAuthContext* ctx,
                                        AdbTransportType type,
                                        const char* public_key,
                                        size_t len) {
    return ctx->NotifyTlsDeviceConnected(type, std::string_view(public_key, len));
}

void adbd_auth_tls_device_disconnected(AdbdAuthContext* ctx,
                                       AdbTransportType type,
                                       uint64_t id) {
    ctx->NotifyTlsDeviceDisconnected(type, id);
}

uint32_t adbd_auth_get_max_version() {
    return kAuthVersion;
}

bool adbd_auth_supports_feature(AdbdAuthFeature f) {
    UNUSED(f);
    return false;
}
