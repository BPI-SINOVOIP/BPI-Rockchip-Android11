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
#define LOG_TAG "pwrstats_util"

#include <android-base/logging.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>

#include <pwrstatsutil.pb.h>
#include "PowerStatsCollector.h"

namespace {
volatile std::sig_atomic_t gSignalStatus;
}

static void signalHandler(int signal) {
    gSignalStatus = signal;
}

class Options {
  public:
    bool humanReadable;
    bool daemonMode;
    std::string filePath;
};

static Options parseArgs(int argc, char** argv) {
    Options opt = {
            .humanReadable = false,
            .daemonMode = false,
    };

    static struct option long_options[] = {/* These options set a flag. */
                                           {"human-readable", no_argument, 0, 0},
                                           {"daemon", required_argument, 0, 'd'},
                                           {0, 0, 0, 0}};

    // getopt_long stores the option index here
    int option_index = 0;

    int c;
    while ((c = getopt_long(argc, argv, "d:", long_options, &option_index)) != -1) {
        switch (c) {
            case 0:
                if ("human-readable" == std::string(long_options[option_index].name)) {
                    opt.humanReadable = true;
                }
                break;
            case 'd':
                opt.daemonMode = true;
                opt.filePath = std::string(optarg);
                break;
            default: /* '?' */
                std::cerr << "pwrstats_util: Prints out device power stats." << std::endl
                          << "--human-readable: human-readable format" << std::endl
                          << "--daemon <path/to/file>, -d <path/to/file>: daemon mode. Spawns a "
                             "daemon process and prints out its <pid>. kill -INT <pid> will "
                             "trigger a write to specified file."
                          << std::endl;
                exit(EXIT_FAILURE);
        }
    }
    return opt;
}

static void snapshot(const Options& opt, const PowerStatsCollector& collector) {
    std::vector<PowerStatistic> stats;
    int ret = collector.get(&stats);
    if (ret) {
        exit(EXIT_FAILURE);
    }

    if (opt.humanReadable) {
        collector.dump(stats, &std::cout);
    } else {
        for (const auto& stat : stats) {
            stat.SerializeToOstream(&std::cout);
        }
    }

    exit(EXIT_SUCCESS);
}

static void daemon(const Options& opt, const PowerStatsCollector& collector) {
    // Following a subset of steps outlined in http://man7.org/linux/man-pages/man7/daemon.7.html

    // Call fork to create child process
    pid_t pid;
    if ((pid = fork()) < 0) {
        LOG(ERROR) << "can't fork" << std::endl;
        exit(EXIT_FAILURE);
    } else if (pid != 0) {
        std::cout << "pid = " << pid << std::endl;
        exit(EXIT_SUCCESS);
    }
    // Daemon process:

    // Get maximum number of file descriptors
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        LOG(ERROR) << "can't get file limit" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Close all open file descriptors
    if (rl.rlim_max == RLIM_INFINITY) {
        rl.rlim_max = 1024;
    }
    for (int i = 0; i < rl.rlim_max; i++) {
        close(i);
    }

    // Detach from any terminal and create an independent session
    if (setsid() < 0) {
        LOG(ERROR) << "SID creation failed";
        exit(EXIT_FAILURE);
    }

    // connect /dev/null to standard input, output, and error.
    int devnull = open("/dev/null", O_RDWR | O_CLOEXEC);
    dup2(devnull, STDIN_FILENO);
    dup2(devnull, STDOUT_FILENO);
    dup2(devnull, STDERR_FILENO);

    // Reset the umask to 0
    umask(0);

    // Change the current directory to the root
    // directory (/), in order to avoid that the daemon involuntarily
    // blocks mount points from being unmounted
    if (chdir("/") < 0) {
        LOG(ERROR) << "can't change directory to /" << std::endl;
        exit(EXIT_FAILURE);
    }

    // Install a signal handler
    std::signal(SIGINT, signalHandler);

    // get the start_data
    auto start_time = std::chrono::system_clock::now();

    std::vector<PowerStatistic> start_stats;
    int ret = collector.get(&start_stats);
    if (ret) {
        LOG(ERROR) << "failed to get start stats";
        exit(EXIT_FAILURE);
    }

    // Wait for INT signal
    while (gSignalStatus != SIGINT) {
        pause();
    }

    // get the end data
    std::vector<PowerStatistic> interval_stats;
    ret = collector.get(start_stats, &interval_stats);
    if (ret) {
        LOG(ERROR) << "failed to get interval stats";
        exit(EXIT_FAILURE);
    }
    auto end_time = std::chrono::system_clock::now();

    std::chrono::duration<double> elapsed_seconds = end_time - start_time;

    // Write data to file
    std::ofstream myfile(opt.filePath, std::ios::out | std::ios::binary);
    if (!myfile.is_open()) {
        LOG(ERROR) << "failed to open file";
        exit(EXIT_FAILURE);
    }
    myfile << "elapsed time: " << elapsed_seconds.count() << "s" << std::endl;
    if (opt.humanReadable) {
        collector.dump(interval_stats, &myfile);
    } else {
        for (const auto& stat : interval_stats) {
            stat.SerializeToOstream(&myfile);
        }
    }

    myfile.close();

    exit(EXIT_SUCCESS);
}

static void runWithOptions(const Options& opt, const PowerStatsCollector& collector) {
    if (opt.daemonMode) {
        daemon(opt, collector);
    } else {
        snapshot(opt, collector);
    }
}

int run(int argc, char** argv, const PowerStatsCollector& collector) {
    Options opt = parseArgs(argc, argv);

    runWithOptions(opt, collector);

    return 0;
}
