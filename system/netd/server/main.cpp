/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <chrono>
#include <cinttypes>
#include <mutex>

#define LOG_TAG "Netd"

#include "log/log.h"

#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <libbpf_android.h>
#include <netdutils/Stopwatch.h>

#include "Controllers.h"
#include "FwmarkServer.h"
#include "MDnsSdListener.h"
#include "NFLogListener.h"
#include "NetdConstants.h"
#include "NetdHwService.h"
#include "NetdNativeService.h"
#include "NetlinkManager.h"
#include "Process.h"

#include "netd_resolv/resolv.h"

using android::IPCThreadState;
using android::sp;
using android::status_t;
using android::String16;
using android::net::FwmarkServer;
using android::net::gCtls;
using android::net::gLog;
using android::net::makeNFLogListener;
using android::net::NetdHwService;
using android::net::NetdNativeService;
using android::net::NetlinkManager;
using android::net::NFLogListener;
using android::netdutils::Stopwatch;

const char* const PID_FILE_PATH = "/data/misc/net/netd_pid";
constexpr const char DNSPROXYLISTENER_SOCKET_NAME[] = "dnsproxyd";

std::mutex android::net::gBigNetdLock;

namespace {

void getNetworkContextCallback(uint32_t netId, uint32_t uid, android_net_context* netcontext) {
    gCtls->netCtrl.getNetworkContext(netId, uid, netcontext);
}

bool checkCallingPermissionCallback(const char* permission) {
    return checkCallingPermission(String16(permission));
}

void logCallback(const char* msg) {
    gLog.info(std::string(msg));
}

int tagSocketCallback(int sockFd, uint32_t tag, uid_t uid, pid_t) {
    // Workaround for secureVPN with VpnIsolation enabled, refer to b/159994981 for details.
    if (tag == TAG_SYSTEM_DNS) uid = AID_DNS;
    return gCtls->trafficCtrl.tagSocket(sockFd, tag, uid, geteuid());
}

bool evaluateDomainNameCallback(const android_net_context&, const char* /*name*/) {
    return true;
}

bool initDnsResolver() {
    ResolverNetdCallbacks callbacks = {
            .check_calling_permission = &checkCallingPermissionCallback,
            .get_network_context = &getNetworkContextCallback,
            .log = &logCallback,
            .tagSocket = &tagSocketCallback,
            .evaluate_domain_name = &evaluateDomainNameCallback,
    };
    return resolv_init(&callbacks);
}

}  // namespace

int main() {
    Stopwatch s;
    gLog.info("netd 1.0 starting");

    android::net::process::removePidFile(PID_FILE_PATH);
    android::net::process::blockSigPipe();

    // Before we do anything that could fork, mark CLOEXEC the UNIX sockets that we get from init.
    // FrameworkListener does this on initialization as well, but we only initialize these
    // components after having initialized other subsystems that can fork.
    for (const auto& sock :
         {DNSPROXYLISTENER_SOCKET_NAME, FwmarkServer::SOCKET_NAME, MDnsSdListener::SOCKET_NAME}) {
        setCloseOnExec(sock);
    }

    // Make sure BPF programs are loaded before doing anything
    android::bpf::waitForProgsLoaded();

    NetlinkManager *nm = NetlinkManager::Instance();
    if (nm == nullptr) {
        ALOGE("Unable to create NetlinkManager");
        exit(1);
    };

    gCtls = new android::net::Controllers();
    gCtls->init();

    if (nm->start()) {
        ALOGE("Unable to start NetlinkManager (%s)", strerror(errno));
        exit(1);
    }

    std::unique_ptr<NFLogListener> logListener;
    {
        auto result = makeNFLogListener();
        if (!isOk(result)) {
            ALOGE("Unable to create NFLogListener: %s", toString(result).c_str());
            exit(1);
        }
        logListener = std::move(result.value());
        auto status = gCtls->wakeupCtrl.init(logListener.get());
        if (!isOk(result)) {
            gLog.error("Unable to init WakeupController: %s", toString(result).c_str());
            // We can still continue without wakeup packet logging.
        }
    }

    // Set local DNS mode, to prevent bionic from proxying
    // back to this service, recursively.
    // TODO: Check if we could remove it since resolver cache no loger
    // checks this environment variable after aosp/838050.
    setenv("ANDROID_DNS_MODE", "local", 1);
    // Note that only call initDnsResolver after gCtls initializing.
    if (!initDnsResolver()) {
        ALOGE("Unable to init resolver");
        exit(1);
    }

    MDnsSdListener mdnsl;
    if (mdnsl.startListener()) {
        ALOGE("Unable to start MDnsSdListener (%s)", strerror(errno));
        exit(1);
    }

    FwmarkServer fwmarkServer(&gCtls->netCtrl, &gCtls->eventReporter, &gCtls->trafficCtrl);
    if (fwmarkServer.startListener()) {
        ALOGE("Unable to start FwmarkServer (%s)", strerror(errno));
        exit(1);
    }

    Stopwatch subTime;
    status_t ret;
    if ((ret = NetdNativeService::start()) != android::OK) {
        ALOGE("Unable to start NetdNativeService: %d", ret);
        exit(1);
    }
    gLog.info("Registering NetdNativeService: %" PRId64 "us", subTime.getTimeAndResetUs());

    android::net::process::ScopedPidFile pidFile(PID_FILE_PATH);

    // Now that netd is ready to process commands, advertise service availability for HAL clients.
    sp<NetdHwService> mHwSvc(new NetdHwService());
    if ((ret = mHwSvc->start()) != android::OK) {
        ALOGE("Unable to start NetdHwService: %d", ret);
        exit(1);
    }
    gLog.info("Registering NetdHwService: %" PRId64 "us", subTime.getTimeAndResetUs());
    gLog.info("Netd started in %" PRId64 "us", s.timeTakenUs());

    IPCThreadState::self()->joinThreadPool();

    gLog.info("netd exiting");

    exit(0);
}
