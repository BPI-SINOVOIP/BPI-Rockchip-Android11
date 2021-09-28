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

#define LOG_TAG "perfstatsd"

#include <perfstatsd.h>
#include <perfstatsd_service.h>

enum MODE { DUMP_HISTORY, SET_OPTION };

android::sp<Perfstatsd> perfstatsdSp;

void *perfstatsdMain(void *) {
    LOG(INFO) << "main thread started";
    perfstatsdSp = new Perfstatsd();

    while (true) {
        perfstatsdSp->refresh();
        perfstatsdSp->pause();
    }
    return NULL;
}

void help(char **argv) {
    std::string usage = argv[0];
    usage = "Usage: " + usage + " [-s][-d][-o]\n" +
            "Options:\n"
            "    -s, start as service\n"
            "    -d, dump perf stats history for dumpstate_board\n"
            "    -o, set key/value option";

    fprintf(stderr, "%s\n", usage.c_str());
}

int startService(void) {
    pthread_t perfstatsdMainThread;
    errno = pthread_create(&perfstatsdMainThread, NULL, perfstatsdMain, NULL);
    if (errno != 0) {
        PLOG(ERROR) << "Failed to create main thread";
        return -1;
    } else {
        pthread_setname_np(perfstatsdMainThread, "perfstatsd_main");
    }

    android::ProcessState::initWithDriver("/dev/vndbinder");

    if (PerfstatsdPrivateService::start() != android::OK) {
        PLOG(ERROR) << "Failed to start perfstatsd service";
        return -1;
    } else
        LOG(INFO) << "perfstatsd_pri_service started";

    android::ProcessState::self()->startThreadPool();
    android::IPCThreadState::self()->joinThreadPool();
    pthread_join(perfstatsdMainThread, NULL);
    return 0;
}

int serviceCall(int mode, const std::string &key, const std::string &value) {
    android::ProcessState::initWithDriver("/dev/vndbinder");

    android::sp<IPerfstatsdPrivate> perfstatsdPrivateService = getPerfstatsdPrivateService();
    if (perfstatsdPrivateService == NULL) {
        PLOG(ERROR) << "Cannot find perfstatsd service.";
        fprintf(stdout, "Cannot find perfstatsd service.\n");
        return -1;
    }

    switch (mode) {
        case DUMP_HISTORY: {
            std::string history;
            LOG(INFO) << "dump perfstats history.";
            if (!perfstatsdPrivateService->dumpHistory(&history).isOk() || history.empty()) {
                PLOG(ERROR) << "perf stats history is not available";
                fprintf(stdout, "perf stats history is not available\n");
                return -1;
            }
            fprintf(stdout, "%s\n", history.c_str());
            break;
        }
        case SET_OPTION:
            LOG(INFO) << "set option: " << key << " , " << value;
            if (!perfstatsdPrivateService
                     ->setOptions(std::forward<const std::string>(key),
                                  std::forward<const std::string>(value))
                     .isOk()) {
                PLOG(ERROR) << "fail to set options";
                fprintf(stdout, "fail to set options\n");
                return -1;
            }
            break;
    }
    return 0;
}

int serviceCall(int mode) {
    std::string empty("");
    return serviceCall(mode, empty, empty);
}

int main(int argc, char **argv) {
    android::base::InitLogging(argv, android::base::LogdLogger(android::base::SYSTEM));

    int c;
    while ((c = getopt(argc, argv, "sdo:h")) != -1) {
        switch (c) {
            case 's':
                return startService();
            case 'd':
                return serviceCall(DUMP_HISTORY);
            case 'o':
                // set options
                if (argc == 4) {
                    std::string key(argv[2]);
                    std::string value(argv[3]);
                    return serviceCall(SET_OPTION, std::move(key), std::move(value));
                }
                FALLTHROUGH_INTENDED;
            case 'h':
                // print usage
                FALLTHROUGH_INTENDED;
            default:
                help(argv);
                return 2;
        }
    }
    return 0;
}
