/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "createns"
#include <log/log.h>

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <limits>
#include <string>
#include <vector>

static const char kNamespacePath[] = "/data/vendor/var/run/netns/";
static const char kProcNsNet[] = "/proc/self/ns/net";

class Fd {
public:
    explicit Fd(int fd) : mFd(fd) { }
    Fd(const Fd&) = delete;
    ~Fd() {
        if (mFd != -1) {
            ::close(mFd);
            mFd = -1;
        }
    }

    int get() const { return mFd; }
    Fd& operator=(const Fd&) = delete;
private:
    int mFd;
};

static void usage(const char* program) {
    ALOGE("%s <namespace>", program);
}

static bool removeFile(const char* file) {
    if (::unlink(file) == -1) {
        ALOGE("Failed to unlink file '%s': %s", file, strerror(errno));
        return false;
    }
    return true;
}

static std::string getNamespacePath(const char* name) {
    size_t len = strlen(name);
    if (len == 0) {
        ALOGE("Must provide a namespace argument that is not empty");
        return std::string();
    }

    if (std::numeric_limits<size_t>::max() - sizeof(kNamespacePath) < len) {
        // The argument is so big the resulting string can't fit in size_t
        ALOGE("Namespace argument too long");
        return std::string();
    }

    std::vector<char> nsPath(sizeof(kNamespacePath) + len);
    size_t totalSize = strlcpy(nsPath.data(), kNamespacePath, nsPath.size());
    if (totalSize >= nsPath.size()) {
        // The resulting string had to be concatenated to fit, this is a logic
        // error in the code above that determines the size of the data.
        ALOGE("Could not create namespace path");
        return std::string();
    }
    totalSize = strlcat(nsPath.data(), name, nsPath.size());
    if (totalSize >= nsPath.size()) {
        // The resulting string had to be concatenated to fit, this is a logic
        // error in the code above that determines the size of the data.
        ALOGE("Could not append to namespace path");
        return std::string();
    }
    return nsPath.data();
}

static bool writeNamespacePid(const char* name, pid_t pid) {
    std::string path = getNamespacePath(name);
    if (path.empty()) {
        return false;
    }
    path += ".pid";

    Fd fd(::open(path.c_str(),
                 O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC,
                 S_IRUSR | S_IWUSR | S_IRGRP));
    if (fd.get() == -1) {
        ALOGE("Unable to create file '%s': %s", path.c_str(), strerror(errno));
        return false;
    }

    // In order to safely print a pid_t we use int64_t with a known format
    // specifier. Ensure that a pid_t will fit in a pid_t. According to POSIX
    // pid_t is signed.
    static_assert(sizeof(pid_t) <= sizeof(int64_t),
                  "pid_t is larger than int64_t");
    char pidString[32];
    int printed = snprintf(pidString,
                           sizeof(pidString),
                           "%" PRId64,
                           static_cast<int64_t>(pid));
    if (printed <= 0) {
        ALOGE("Unabled to created PID string for writing");
        removeFile(path.c_str());
        return false;
    }

    const char* toPrint = pidString;
    int remaining = printed;
    for (;;) {
        int result = ::write(fd.get(), toPrint, remaining);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            ALOGE("Unable to write pid to file %s: %s",
                  path.c_str(), strerror(errno));
            removeFile(path.c_str());
            return false;
        } else if (result < printed) {
            remaining -= result;
            toPrint += result;
        } else {
            break;
        }
    }
    return true;
}

static pid_t daemonize(int fd) {
    // This convoluted way of demonizing the process is described in
    // man (7) daemon.

    // (1) Close all files, we don't have any open files at this point
    // (2) Reset all signal handlers to default, they already are
    // (3) Reset the signal mask, we never changed it
    // (4) Sanitize environment block, we didn't change the environment
    // (5) Call fork
    pid_t pid = ::fork();
    if (pid != 0) {
        // In the parent, nothing more to do
        return pid;
    }

    // (6) Acquire a new session to detach from terminal
    ::setsid();

    // (7) Fork again to avoid the daemon being attached to a terminal again
    pid = ::fork();
    if (pid != 0) {
        // (8) This is the first child, needs to call exit
        exit(0);
        return pid;
    }
    // (9) Connect /dev/null to stdin, stdout, stderr
    ::close(STDIN_FILENO);
    ::close(STDOUT_FILENO);
    ::close(STDERR_FILENO);
    // Since open will always reuse the lowest available fd and we have closed
    // every single fd at this point we can just open them in the correct order.
    if (::open("/dev/null", O_RDONLY) == -1) {
        ALOGE("Unable to open /dev/null as stdin");
    }
    if (::open("/dev/null", O_WRONLY) == -1) {
        ALOGE("Unable to open /dev/null as stdout");
    }
    if (::open("/dev/null", O_WRONLY) == -1) {
        ALOGE("Unable to open /dev/null as stderr");
    }
    // (10) Reset umask to zero
    ::umask(0);
    // (11) Change directory to root (/)
    if (::chdir("/") != 0) {
        ALOGE("Failed to set working directory to root: %s", strerror(errno));
    }
    // (12) Write the pid of the daemon to a file, we're passing this to
    // the process that starts the daemon to ensure that the pid file exists
    // once that process exits. Atomicity is guaranteed by that write requiring
    // that the pid file does not exist to begin with.
    pid = ::getpid();
    if (::write(fd, &pid, sizeof(pid)) != sizeof(pid)) {
        ALOGE("Unable to write pid to pipe: %s", strerror(errno));
        ::close(fd);
        exit(1);
    }
    ::close(fd);
    // (13) Drop privileges, doing this causes problems for execns when it's
    // trying to open the proc/ns/net file of this process so we can't do that.
    // (14) Notify the starting process that the daemon is running, this is done
    // in step (12) above.
    // (15) Exit starting process happens in main where it returns.
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        ALOGE("Failed to create pipe: %s", strerror(errno));
        return 1;
    }

    Fd readPipe(fds[0]);
    Fd writePipe(fds[1]);

    if (::unshare(CLONE_NEWNET) != 0) {
        ALOGE("Failed to create network namespace '%s': %s",
              argv[1],
              strerror(errno));
        return 1;
    }

    std::string path = getNamespacePath(argv[1]);
    if (path.empty()) {
        return 1;
    }
    {
        // Open and then immediately close the fd
        Fd fd(::open(path.c_str(), O_CREAT | O_TRUNC | O_RDONLY | O_CLOEXEC,
                     S_IRUSR | S_IWUSR | S_IRGRP));
        if (fd.get() == -1) {
            ALOGE("Failed to open file %s: %s", path.c_str(), strerror(errno));
            return 1;
        }
    }
    if (::mount(kProcNsNet, path.c_str(), nullptr, MS_BIND, nullptr) != 0) {
        ALOGE("Failed to bind %s to %s: %s",
              kProcNsNet,
              path.c_str(),
              strerror(errno));
        // Clean up on failure
        removeFile(path.c_str());
        return 1;
    }

    // At this point we fork. This way we keep a process in the namespace alive
    // without this command being blocking. This is valuable because it allows
    // us to write the pid to a file before we exit. That way we can guarantee
    // that after this command completes there is a pid to be read, there is no
    // asynchronous behavior going on.
    pid_t pid = daemonize(writePipe.get());
    if (pid == 0) {
        // In the child
        for (;;) {
            pause();
        }
    } else {
        // In the parent, read the pid of the daemon from the pipe and write it
        // to a file.
        pid_t child = 0;
        if (::read(readPipe.get(), &child, sizeof(child)) != sizeof(child)) {
            ALOGE("Failed to read child PID from pipe: %s", strerror(errno));
            return 1;
        }
        if (!writeNamespacePid(argv[1], child)) {
            return 1;
        }
    }

    return 0;
}

