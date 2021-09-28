#include <android/binder_process.h>
#include <utils/Errors.h>
#include <utils/Log.h>

#include <iostream>
#include <thread>

#include "RouterSvc.h"

using namespace android::automotive::computepipe::router::V1_0::implementation;

static RouterSvc gSvcInstance;

#define SVC_NAME() gSvcInstance.getSvcName().c_str()

static void startService() {
    auto status = gSvcInstance.initSvc();
    if (status != router::Error::OK) {
        ALOGE("Could not register service %s", SVC_NAME());
        exit(2);
    }
    ALOGE("Registration Complete");
}

int main(int argc, char** argv) {
    auto status = gSvcInstance.parseArgs(argc - 1, &argv[1]);
    if (status != router::Error::OK) {
        ALOGE("Bad Args");
        exit(2);
    }
    ABinderProcess_startThreadPool();
    std::thread registrationThread(startService);
    ABinderProcess_joinThreadPool();
    ALOGE("Router thread joined IPC pool");
    return 1;
}
