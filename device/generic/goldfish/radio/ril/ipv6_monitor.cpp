/*
 * Copyright 2019, The Android Open Source Project
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

#include "ipv6_monitor.h"

#include <errno.h>
#include <linux/filter.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>

#include <array>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#define LOG_TAG "RIL-IPV6MON"
#include <utils/Log.h>

static constexpr size_t kReadBufferSize = 32768;

static constexpr size_t kRecursiveDnsOptHeaderSize = 8;

static constexpr size_t kControlClient = 0;
static constexpr size_t kControlServer = 1;

static constexpr char kMonitorAckCommand = '\1';
static constexpr char kMonitorStopCommand = '\2';

// The amount of time to wait before trying to initialize interface again if
// it's not ready when rild starts.
static constexpr int kDeferredTimeoutMilliseconds = 1000;

bool operator==(const in6_addr& left, const in6_addr& right) {
    return ::memcmp(left.s6_addr, right.s6_addr, sizeof(left.s6_addr)) == 0;
}

bool operator!=(const in6_addr& left, const in6_addr& right) {
    return ::memcmp(left.s6_addr, right.s6_addr, sizeof(left.s6_addr)) != 0;
}

template<class T>
static inline void hash_combine(size_t& seed, const T& value) {
    std::hash<T> hasher;
    seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std {
template<> struct hash<in6_addr> {
    size_t operator()(const in6_addr& ad) const {
        size_t seed = 0;
        hash_combine(seed, *reinterpret_cast<const uint32_t*>(&ad.s6_addr[0]));
        hash_combine(seed, *reinterpret_cast<const uint32_t*>(&ad.s6_addr[4]));
        hash_combine(seed, *reinterpret_cast<const uint32_t*>(&ad.s6_addr[8]));
        hash_combine(seed, *reinterpret_cast<const uint32_t*>(&ad.s6_addr[12]));
        return seed;
    }
};
}  // namespace std

static constexpr uint32_t kIpTypeOffset = offsetof(ip6_hdr, ip6_nxt);
static constexpr uint32_t kIcmpTypeOffset = sizeof(ip6_hdr) +
                                            offsetof(icmp6_hdr, icmp6_type); 

// This is BPF program that will filter out anything that is not an NDP router
// advertisement. It's a very basic assembler syntax. The jumps indicate how
// many instructions to jump in addition to the automatic increment of the
// program counter. So a jump statement with a zero means to go to the next
// instruction, a value of 3 means that the next instruction will be the 4th
// after the current one.
static const struct sock_filter kNdpFilter[] = {
    // Load byte at absolute address kIpTypeOffset
    BPF_STMT(BPF_LD | BPF_B | BPF_ABS, kIpTypeOffset),
    // Jump, if byte is IPPROTO_ICMPV6 jump 0 instructions, if not jump 3.
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, IPPROTO_ICMPV6, 0, 3),
    // Load byte at absolute address kIcmpTypeOffset
    BPF_STMT(BPF_LD | BPF_B | BPF_ABS, kIcmpTypeOffset),
    // Jump, if byte is ND_ROUTER_ADVERT jump 0 instructions, if not jump 1
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ND_ROUTER_ADVERT, 0, 1),
    // Return the number of bytes to accept, accept all of them
    BPF_STMT(BPF_RET | BPF_K, std::numeric_limits<uint32_t>::max()),
    // Accept zero bytes, this is where the failed jumps go
    BPF_STMT(BPF_RET | BPF_K, 0)
};
static constexpr size_t kNdpFilterSize =
    sizeof(kNdpFilter) / sizeof(kNdpFilter[0]);

class Ipv6Monitor {
public:
    Ipv6Monitor(const char* interfaceName);
    ~Ipv6Monitor();

    enum class InitResult {
        Error,
        Deferred,
        Success,
    };
    InitResult init();
    void setCallback(ipv6MonitorCallback callback);
    void runAsync();
    void stop();

private:
    InitResult initInterfaces();
    void run();
    void onReadAvailable();

    ipv6MonitorCallback mMonitorCallback;

    in6_addr mGateway;
    std::unordered_set<in6_addr> mDnsServers;

    std::unique_ptr<std::thread> mThread;
    std::mutex mThreadMutex;

    std::string mInterfaceName;
    int mSocketFd;
    int mControlSocket[2];
    int mPollTimeout = -1;
    bool mFullyInitialized = false;
};

Ipv6Monitor::Ipv6Monitor(const char* interfaceName) :
    mMonitorCallback(nullptr),
    mInterfaceName(interfaceName),
    mSocketFd(-1) {
    memset(&mGateway, 0, sizeof(mGateway));
    mControlSocket[0] = -1;
    mControlSocket[1] = -1;
}

Ipv6Monitor::~Ipv6Monitor() {
    for (int& fd : mControlSocket) {
        if (fd != -1) {
            ::close(fd);
            fd = -1;
        }
    }
    if (mSocketFd != -1) {
        ::close(mSocketFd);
        mSocketFd = -1;
    }
}

Ipv6Monitor::InitResult Ipv6Monitor::init() {
    if (mSocketFd != -1) {
        RLOGE("Ipv6Monitor already initialized");
        return InitResult::Error;
    }

    if (::socketpair(AF_UNIX, SOCK_DGRAM, 0, mControlSocket) != 0) {
        RLOGE("Ipv6Monitor failed to create control socket pair: %s",
              strerror(errno));
        return InitResult::Error;
    }

    mSocketFd = ::socket(AF_PACKET, SOCK_DGRAM | SOCK_CLOEXEC, ETH_P_IPV6);
    if (mSocketFd == -1) {
        RLOGE("Ipv6Monitor failed to open socket: %s", strerror(errno));
        return InitResult::Error;
    }
    // If interface initialization fails we'll retry later
    return initInterfaces();
}

void Ipv6Monitor::setCallback(ipv6MonitorCallback callback) {
    mMonitorCallback = callback;
}

Ipv6Monitor::InitResult Ipv6Monitor::initInterfaces() {
    if (mFullyInitialized) {
        RLOGE("Ipv6Monitor already initialized");
        return InitResult::Error;
    }
    struct ifreq request;
    memset(&request, 0, sizeof(request));
    strlcpy(request.ifr_name, mInterfaceName.c_str(), sizeof(request.ifr_name));

    // Set the ALLMULTI flag so we can capture multicast traffic
    int status = ::ioctl(mSocketFd, SIOCGIFFLAGS, &request);
    if (status != 0) {
        if (errno == ENODEV) {
            // It is not guaranteed that the network is entirely set up by the
            // time rild has started. If that's the case the radio interface
            // might not be up yet, try again later.
            RLOGE("Ipv6Monitor could not initialize %s yet, retrying later",
                  mInterfaceName.c_str());
            mPollTimeout = kDeferredTimeoutMilliseconds;
            return InitResult::Deferred;
        }
        RLOGE("Ipv6Monitor failed to get interface flags for %s: %s",
              mInterfaceName.c_str(), strerror(errno));
        return InitResult::Error;
    }

    if ((request.ifr_flags & IFF_ALLMULTI) == 0) {
        // The flag is not set, we have to make another call
        request.ifr_flags |= IFF_ALLMULTI;

        status = ::ioctl(mSocketFd, SIOCSIFFLAGS, &request);
        if (status != 0) {
            RLOGE("Ipv6Monitor failed to set interface flags for %s: %s",
                  mInterfaceName.c_str(), strerror(errno));
            return InitResult::Error;
        }
    }

    // Add a BPF filter to the socket so that we only receive the specific
    // type of packet we're interested in. Otherwise we will receive ALL
    // traffic on this interface.
    struct sock_fprog filter;
    filter.len = kNdpFilterSize;
    // The API doesn't have const but it's not going to modify it so this is OK
    filter.filter = const_cast<struct sock_filter*>(kNdpFilter);
    status = ::setsockopt(mSocketFd,
                          SOL_SOCKET,
                          SO_ATTACH_FILTER,
                          &filter,
                          sizeof(filter));
    if (status != 0) {
        RLOGE("Ipv6Monitor failed to set socket filter: %s", strerror(errno));
        return InitResult::Error;
    }

    // Get the hardware address of the interface into a sockaddr struct for bind
    struct sockaddr_ll ethAddr;
    memset(&ethAddr, 0, sizeof(ethAddr));
    ethAddr.sll_family = AF_PACKET;
    ethAddr.sll_protocol = htons(ETH_P_IPV6);
    ethAddr.sll_ifindex = if_nametoindex(mInterfaceName.c_str());
    if (ethAddr.sll_ifindex == 0) {
        RLOGE("Ipv6Monitor failed to find index for %s: %s",
              mInterfaceName.c_str(), strerror(errno));
        return InitResult::Error;
    }

    status = ::ioctl(mSocketFd, SIOCGIFHWADDR, &request);
    if (status != 0) {
        RLOGE("Ipv6Monitor failed to get hardware address for %s: %s",
              mInterfaceName.c_str(), strerror(errno));
        return InitResult::Error;
    }
    memcpy(ethAddr.sll_addr, request.ifr_addr.sa_data, ETH_ALEN);

    // Now bind to the hardware address
    status = ::bind(mSocketFd,
                    reinterpret_cast<const struct sockaddr*>(&ethAddr),
                    sizeof(ethAddr));
    if (status != 0) {
        RLOGE("Ipv6Monitor failed to bind to %s hardware address: %s",
              mInterfaceName.c_str(), strerror(errno));
        return InitResult::Error;
    }
    mFullyInitialized = true;
    return InitResult::Success;
}

void Ipv6Monitor::runAsync() {
    std::unique_lock<std::mutex> lock(mThreadMutex);
    mThread = std::make_unique<std::thread>([this]() { run(); });
}

void Ipv6Monitor::stop() {
    std::unique_lock<std::mutex> lock(mThreadMutex);
    if (!mThread) {
        return;
    }
    ::write(mControlSocket[kControlClient], &kMonitorStopCommand, 1);
    char ack = -1;
    while (ack != kMonitorAckCommand) {
        ::read(mControlSocket[kControlClient], &ack, sizeof(ack));
    }
    mThread->join();
    mThread.reset();
}

void Ipv6Monitor::run() {
    std::array<struct pollfd, 2> fds;
    fds[0].events = POLLIN;
    fds[0].fd = mControlSocket[kControlServer];
    fds[1].events = POLLIN;
    fds[1].fd = mSocketFd;

    bool running = true;
    while (running) {
        int status = ::poll(fds.data(), fds.size(), mPollTimeout);
        if (status < 0) {
            if (errno == EINTR) {
                // Interrupted, keep going
                continue;
            }
            // An error occurred
            RLOGE("Ipv6Monitor fatal failure polling failed; %s",
                  strerror(errno));
            break;
        } else if (status == 0) {
            // Timeout, nothing to read
            if (!mFullyInitialized) {
                InitResult result = initInterfaces();
                switch (result) {
                    case InitResult::Error:
                        // Something went wrong this time and we can't recover
                        running = false;
                        break;
                    case InitResult::Deferred:
                        // We need to keep waiting and then try again
                        mPollTimeout = kDeferredTimeoutMilliseconds;
                        break;
                    case InitResult::Success:
                        // Interfaces are initialized, no need to timeout again
                        mPollTimeout = -1;
                        break;
                }
            }
            continue;
        }

        if (fds[0].revents & POLLIN) {
            // Control message received
            char command = -1;
            if (::read(mControlSocket[kControlServer],
                       &command,
                       sizeof(command)) == 1) {
                if (command == kMonitorStopCommand) {
                    break;
                }
            }
        } else if (fds[1].revents & POLLIN) {
            onReadAvailable();
        }
    }
    ::write(mControlSocket[kControlServer], &kMonitorAckCommand, 1);
}

void Ipv6Monitor::onReadAvailable() {
    char buffer[kReadBufferSize];

    ssize_t bytesRead = 0;
    while (true) {
        bytesRead = ::recv(mSocketFd, buffer, sizeof(buffer), 0);
        if (bytesRead < 0) {
            if (errno == EINTR) {
                // Interrupted, try again right away
                continue;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                // Do not report an error for the above error codes, they are
                // part of the normal turn of events. We just need to try again
                // later when we run into those errors.
                RLOGE("Ipv6Monitor failed to receive data: %s",
                      strerror(errno));
            }
            return;
        }
        break;
    }

    if (mMonitorCallback == nullptr) {
        // No point in doing anything, we have read the data so the socket
        // buffer doesn't fill up and that's all we can do.
        return;
    }

    if (static_cast<size_t>(bytesRead) < sizeof(ip6_hdr) + sizeof(icmp6_hdr)) {
        // This message cannot be an ICMPv6 packet, ignore it
        return;
    }

    auto ipv6 = reinterpret_cast<const ip6_hdr*>(buffer);
    uint8_t version = (ipv6->ip6_vfc & 0xF0) >> 4;
    if (version != 6 || ipv6->ip6_nxt != IPPROTO_ICMPV6) {
        // This message is not an IPv6 packet or not an ICMPv6 packet, ignore it
        return;
    }

    // The ICMP header starts right after the IPv6 header
    auto icmp = reinterpret_cast<const icmp6_hdr*>(buffer + sizeof(ip6_hdr));
    if (icmp->icmp6_code != 0) {
        // All packets we care about have an icmp code of zero.
        return;
    }

    if (icmp->icmp6_type != ND_ROUTER_ADVERT) {
        // We only care about router advertisements
        return;
    }

    // At this point we know it's a valid packet, let's look inside

    // The gateway is the same as the source in the IP header
    in6_addr gateway = ipv6->ip6_src;

    // Search through the options for DNS servers
    const char* options = buffer + sizeof(ip6_hdr) + sizeof(nd_router_advert);
    const nd_opt_hdr* option = reinterpret_cast<const nd_opt_hdr*>(options);

    std::vector<in6_addr> dnsServers;
    const nd_opt_hdr* nextOpt = nullptr;
    for (const nd_opt_hdr* opt = option; opt; opt = nextOpt) {
        auto nextOptLoc =
            reinterpret_cast<const char*>(opt) + opt->nd_opt_len * 8u;
        if (nextOptLoc > buffer + bytesRead) {
            // Not enough room for this option, abort
            break;
        }
        if (nextOptLoc < buffer + bytesRead) {
            nextOpt = reinterpret_cast<const nd_opt_hdr*>(nextOptLoc);
        } else {
            nextOpt = nullptr;
        }
        if (opt->nd_opt_type != 25 || opt->nd_opt_len < 1) {
            // Not an RNDSS option, skip it
            continue;
        }

        size_t numEntries = (opt->nd_opt_len - 1) / 2;
        const char* addrLoc = reinterpret_cast<const char*>(opt);
        addrLoc += kRecursiveDnsOptHeaderSize;
        auto addrs = reinterpret_cast<const in6_addr*>(addrLoc);

        for (size_t i = 0; i < numEntries; ++i) {
            dnsServers.push_back(addrs[i]);
        }
    }

    bool changed = false;
    if (gateway != mGateway) {
        changed = true;
        mGateway = gateway;
    }

    for (const auto& dns : dnsServers) {
        if (mDnsServers.find(dns) == mDnsServers.end()) {
            mDnsServers.insert(dns);
            changed = true;
        }
    }

    if (changed) {
        mMonitorCallback(&gateway, dnsServers.data(), dnsServers.size());
    }
}

extern "C"
struct ipv6Monitor* ipv6MonitorCreate(const char* interfaceName) {
    auto monitor = std::make_unique<Ipv6Monitor>(interfaceName);
    if (!monitor || monitor->init() == Ipv6Monitor::InitResult::Error) {
        return nullptr;
    }
    return reinterpret_cast<struct ipv6Monitor*>(monitor.release());
}

extern "C"
void ipv6MonitorFree(struct ipv6Monitor* ipv6Monitor) {
    auto monitor = reinterpret_cast<Ipv6Monitor*>(ipv6Monitor);
    delete monitor;
}

extern "C"
void ipv6MonitorSetCallback(struct ipv6Monitor* ipv6Monitor,
                            ipv6MonitorCallback callback) {
    auto monitor = reinterpret_cast<Ipv6Monitor*>(ipv6Monitor);
    monitor->setCallback(callback);
}

extern "C"
void ipv6MonitorRunAsync(struct ipv6Monitor* ipv6Monitor) {
    auto monitor = reinterpret_cast<Ipv6Monitor*>(ipv6Monitor);
    monitor->runAsync();
}

extern "C"
void ipv6MonitorStop(struct ipv6Monitor* ipv6Monitor) {
    auto monitor = reinterpret_cast<Ipv6Monitor*>(ipv6Monitor);
    monitor->stop();
}

