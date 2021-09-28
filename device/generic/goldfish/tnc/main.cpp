
#include <errno.h>
#include <netdb.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <initializer_list>

static void usage(const char* program) {
    fprintf(stderr, "Usage: %s [-s|-c|-b] <ip> <port>\n", program);
}

enum class Mode {
    Bridge,
    Client,
    Server,
};

bool resolve(const char* name, const char* port, struct addrinfo** addrs) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int res = ::getaddrinfo(name, port, &hints, addrs);
    if (res != 0) {
        fprintf(stderr, "ERROR: Unable to resolve '%s' and port '%s': %s\n",
                name, port, gai_strerror(res));
        return false;
    }
    return true;
}

int runClient(struct addrinfo* addrs) {
    int fd = -1;
    for (struct addrinfo* addr = addrs; addr != nullptr; addr = addr->ai_next) {
        fd = ::socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (::connect(fd, addr->ai_addr, addr->ai_addrlen) == 0) {
            break;
        }
        ::close(fd);
    }
    ::freeaddrinfo(addrs);
    if (fd < 0) {
        fprintf(stderr, "Unable to connect to server\n");
        return 1;
    }
    if (::send(fd, "boop", 4, 0) != 4) {
        ::close(fd);
        fprintf(stderr, "Failed to send message to server\n");
        return 1;
    }
    ::close(fd);
    return 0;
}

int runServer(struct addrinfo* addrs) {
    int fd = -1;
    for (struct addrinfo* addr = addrs; addr != nullptr; addr = addr->ai_next) {
        fd = ::socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (::bind(fd, addr->ai_addr, addr->ai_addrlen) == 0) {
            break;
        }
        ::close(fd);
    }
    ::freeaddrinfo(addrs);
    if (fd < 0) {
        fprintf(stderr, "Unable to bind to address\n");
        return 1;
    }
    char buffer[1024];
    for (;;) {
        struct sockaddr_storage addr;
        socklen_t addrSize = sizeof(addr);
        ssize_t bytesRead = recvfrom(fd, buffer, sizeof(buffer), 0,
                                     reinterpret_cast<struct sockaddr*>(&addr),
                                     &addrSize);
        if (bytesRead < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "Error receiving on socket: %s\n", strerror(errno));
            ::close(fd);
            return 1;
        } else if (bytesRead == 0) {
            fprintf(stderr, "Socket unexpectedly closed\n");
            ::close(fd);
            return 1;
        }
        printf("Received message from client '%*s'\n",
               static_cast<int>(bytesRead), buffer);
    }
}

static const char kBridgeName[] = "br0";

static int configureBridge() {
    int fd = ::socket(AF_LOCAL, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Could not open bridge socket: %s\n",
                strerror(errno));
        return 1;
    }

    int res = ::ioctl(fd, SIOCBRADDBR, kBridgeName);
    if (res < 0) {
        fprintf(stderr, "ERROR: cannot create bridge: %s\n", strerror(errno));
        ::close(fd);
        return 1;
    }

    for (const auto& ifName : { "eth0", "wlan1", "radio0-peer" }) {
        struct ifreq request;
        memset(&request, 0, sizeof(request));
        request.ifr_ifindex = if_nametoindex(ifName);
        if (request.ifr_ifindex == 0) {
            fprintf(stderr, "ERROR: Unable to get interface index for %s\n",
                    ifName);
            ::close(fd);
            return 1;
        }
        strlcpy(request.ifr_name, kBridgeName, sizeof(request.ifr_name));
        res = ::ioctl(fd, SIOCBRADDIF, &request);
        if (res < 0) {
            fprintf(stderr, "ERROR: cannot add if %s to bridge: %s\n",
                    ifName, strerror(errno));
            ::close(fd);
            return 1;
        }
    }

    struct ifreq request;
    memset(&request, 0, sizeof(request));
    request.ifr_ifindex = if_nametoindex(kBridgeName);
    if (request.ifr_ifindex == 0) {
        fprintf(stderr, "ERROR: Unable to get interface index for %s\n",
                kBridgeName);
        ::close(fd);
        return 1;
    }
    strlcpy(request.ifr_name, kBridgeName, sizeof(request.ifr_name));
    res = ::ioctl(fd, SIOCGIFFLAGS, &request);
    if (res != 0) {
        fprintf(stderr, "ERROR: Unable to get interface index for %s\n",
                kBridgeName);
        ::close(fd);
        return 1;
    }
    if ((request.ifr_flags & IFF_UP) == 0) {
        // Bridge is not up, it needs to be up to work
        request.ifr_flags |= IFF_UP;
        res = ::ioctl(fd, SIOCSIFFLAGS, &request);
        if (res != 0) {
            fprintf(stderr, "ERROR: Unable to set interface flags for %s\n",
                    kBridgeName);
            ::close(fd);
            return 1;
        }
    }

    ::close(fd);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    Mode mode;
    if (strcmp("-b", argv[1]) == 0) {
        mode = Mode::Bridge;
    } else if (strcmp("-c", argv[1]) == 0) {
        mode = Mode::Client;
    } else if (strcmp("-s", argv[1]) == 0) {
        mode = Mode::Server;
    } else {
        fprintf(stderr, "ERROR: Invalid option '%s'\n", argv[1]);
        usage(argv[0]);
        return 1;
    }

    struct addrinfo* addrs = nullptr;
    if (mode == Mode::Client || mode == Mode::Server) {
        if (argc != 4) {
            usage(argv[0]);
            return 1;
        }
        if (!resolve(argv[2], argv[3], &addrs)) {
            usage(argv[0]);
            return 1;
        }
    }

    switch (mode) {
        case Mode::Bridge:
            return configureBridge();
        case Mode::Client:
            return runClient(addrs);
        case Mode::Server:
            return runServer(addrs);
    }
}

