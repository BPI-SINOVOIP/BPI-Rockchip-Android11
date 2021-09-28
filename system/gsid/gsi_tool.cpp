//
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <getopt.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <android/gsi/IGsiService.h>
#include <binder/IServiceManager.h>
#include <cutils/android_reboot.h>
#include <libgsi/libgsi.h>
#include <libgsi/libgsid.h>

using namespace android::gsi;
using namespace std::chrono_literals;

using android::sp;
using android::base::Split;
using android::base::StringPrintf;
using CommandCallback = std::function<int(sp<IGsiService>, int, char**)>;

static int Disable(sp<IGsiService> gsid, int argc, char** argv);
static int Enable(sp<IGsiService> gsid, int argc, char** argv);
static int Install(sp<IGsiService> gsid, int argc, char** argv);
static int Wipe(sp<IGsiService> gsid, int argc, char** argv);
static int WipeData(sp<IGsiService> gsid, int argc, char** argv);
static int Status(sp<IGsiService> gsid, int argc, char** argv);
static int Cancel(sp<IGsiService> gsid, int argc, char** argv);

static const std::map<std::string, CommandCallback> kCommandMap = {
        // clang-format off
        {"disable", Disable},
        {"enable", Enable},
        {"install", Install},
        {"wipe", Wipe},
        {"wipe-data", WipeData},
        {"status", Status},
        {"cancel", Cancel},
        // clang-format on
};

static std::string ErrorMessage(const android::binder::Status& status,
                                int error_code = IGsiService::INSTALL_ERROR_GENERIC) {
    if (!status.isOk()) {
        return status.exceptionMessage().string();
    }
    return "error code " + std::to_string(error_code);
}

class ProgressBar {
  public:
    explicit ProgressBar(sp<IGsiService> gsid) : gsid_(gsid) {}

    ~ProgressBar() { Stop(); }

    void Display() {
        Finish();
        done_ = false;
        last_update_ = {};
        worker_ = std::make_unique<std::thread>([this]() { Worker(); });
    }

    void Stop() {
        if (!worker_) {
            return;
        }
        SignalDone();
        worker_->join();
        worker_ = nullptr;
    }

    void Finish() {
        if (!worker_) {
            return;
        }
        Stop();
        FinishLastBar();
    }

  private:
    void Worker() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (!done_) {
            if (!UpdateProgress()) {
                return;
            }
            cv_.wait_for(lock, 500ms, [this] { return done_; });
        }
    }

    bool UpdateProgress() {
        GsiProgress latest;
        auto status = gsid_->getInstallProgress(&latest);
        if (!status.isOk()) {
            std::cout << std::endl;
            return false;
        }
        if (latest.status == IGsiService::STATUS_NO_OPERATION) {
            return true;
        }
        if (last_update_.step != latest.step) {
            FinishLastBar();
        }
        Display(latest);
        return true;
    }

    void FinishLastBar() {
        // If no bar was in progress, don't do anything.
        if (last_update_.total_bytes == 0) {
            return;
        }
        // Ensure we finish the display at 100%.
        last_update_.bytes_processed = last_update_.total_bytes;
        Display(last_update_);
        std::cout << std::endl;
    }

    void Display(const GsiProgress& progress) {
        if (progress.total_bytes == 0) {
            return;
        }

        static constexpr int kColumns = 80;
        static constexpr char kRedColor[] = "\x1b[31m";
        static constexpr char kGreenColor[] = "\x1b[32m";
        static constexpr char kResetColor[] = "\x1b[0m";

        int percentage = (progress.bytes_processed * 100) / progress.total_bytes;
        int64_t bytes_per_col = progress.total_bytes / kColumns;
        uint32_t fill_count = progress.bytes_processed / bytes_per_col;
        uint32_t dash_count = kColumns - fill_count;
        std::string fills = std::string(fill_count, '=');
        std::string dashes = std::string(dash_count, '-');

        // Give the end of the bar some flare.
        if (!fills.empty() && !dashes.empty()) {
            fills[fills.size() - 1] = '>';
        }

        fprintf(stdout, "\r%-15s%6d%% ", progress.step.c_str(), percentage);
        fprintf(stdout, "%s[%s%s%s", kGreenColor, fills.c_str(), kRedColor, dashes.c_str());
        fprintf(stdout, "%s]%s", kGreenColor, kResetColor);
        fflush(stdout);

        last_update_ = progress;
    }

    void SignalDone() {
        std::lock_guard<std::mutex> guard(mutex_);
        done_ = true;
        cv_.notify_all();
    }

  private:
    sp<IGsiService> gsid_;
    std::unique_ptr<std::thread> worker_;
    std::condition_variable cv_;
    std::mutex mutex_;
    GsiProgress last_update_;
    bool done_ = false;
};

static int Install(sp<IGsiService> gsid, int argc, char** argv) {
    constexpr const char* kDefaultPartition = "system";
    struct option options[] = {
            {"install-dir", required_argument, nullptr, 'i'},
            {"gsi-size", required_argument, nullptr, 's'},
            {"no-reboot", no_argument, nullptr, 'n'},
            {"userdata-size", required_argument, nullptr, 'u'},
            {"partition-name", required_argument, nullptr, 'p'},
            {"wipe", no_argument, nullptr, 'w'},
            {nullptr, 0, nullptr, 0},
    };

    int64_t gsiSize = 0;
    int64_t userdataSize = 0;
    bool wipeUserdata = false;
    bool reboot = true;
    std::string installDir = "";
    std::string partition = kDefaultPartition;
    if (getuid() != 0) {
        std::cerr << "must be root to install a GSI" << std::endl;
        return EX_NOPERM;
    }

    int rv, index;
    while ((rv = getopt_long_only(argc, argv, "", options, &index)) != -1) {
        switch (rv) {
            case 'p':
                partition = optarg;
                break;
            case 's':
                if (!android::base::ParseInt(optarg, &gsiSize) || gsiSize <= 0) {
                    std::cerr << "Could not parse image size: " << optarg << std::endl;
                    return EX_USAGE;
                }
                break;
            case 'u':
                if (!android::base::ParseInt(optarg, &userdataSize) || userdataSize < 0) {
                    std::cerr << "Could not parse image size: " << optarg << std::endl;
                    return EX_USAGE;
                }
                break;
            case 'i':
                installDir = optarg;
                break;
            case 'w':
                wipeUserdata = true;
                break;
            case 'n':
                reboot = false;
                break;
        }
    }

    if (gsiSize <= 0) {
        std::cerr << "Must specify --gsi-size." << std::endl;
        return EX_USAGE;
    }

    bool running_gsi = false;
    gsid->isGsiRunning(&running_gsi);
    if (running_gsi) {
        std::cerr << "Cannot install a GSI within a live GSI." << std::endl;
        std::cerr << "Use gsi_tool disable or wipe and reboot first." << std::endl;
        return EX_SOFTWARE;
    }

    android::base::unique_fd input(dup(1));
    if (input < 0) {
        std::cerr << "Error duplicating descriptor: " << strerror(errno) << std::endl;
        return EX_SOFTWARE;
    }
    // Note: the progress bar needs to be re-started in between each call.
    ProgressBar progress(gsid);
    progress.Display();
    int error;
    auto status = gsid->openInstall(installDir, &error);
    if (!status.isOk() || error != IGsiService::INSTALL_OK) {
        std::cerr << "Could not open DSU installation: " << ErrorMessage(status, error) << "\n";
        return EX_SOFTWARE;
    }
    if (partition == kDefaultPartition) {
        auto status = gsid->createPartition("userdata", userdataSize, false, &error);
        if (!status.isOk() || error != IGsiService::INSTALL_OK) {
            std::cerr << "Could not start live image install: " << ErrorMessage(status, error)
                      << "\n";
            return EX_SOFTWARE;
        }
    }

    status = gsid->createPartition(partition, gsiSize, true, &error);
    if (!status.isOk() || error != IGsiService::INSTALL_OK) {
        std::cerr << "Could not start live image install: " << ErrorMessage(status, error) << "\n";
        return EX_SOFTWARE;
    }
    android::os::ParcelFileDescriptor stream(std::move(input));

    bool ok = false;
    progress.Display();
    status = gsid->commitGsiChunkFromStream(stream, gsiSize, &ok);
    if (!ok) {
        std::cerr << "Could not commit live image data: " << ErrorMessage(status) << "\n";
        return EX_SOFTWARE;
    }

    status = gsid->closeInstall(&error);
    if (!status.isOk() || error != IGsiService::INSTALL_OK) {
        std::cerr << "Could not close DSU installation: " << ErrorMessage(status, error) << "\n";
        return EX_SOFTWARE;
    }
    progress.Finish();
    std::string dsuSlot;
    status = gsid->getActiveDsuSlot(&dsuSlot);
    if (!status.isOk()) {
        std::cerr << "Could not get the active DSU slot: " << ErrorMessage(status) << "\n";
        return EX_SOFTWARE;
    }
    status = gsid->enableGsi(true, dsuSlot, &error);
    if (!status.isOk() || error != IGsiService::INSTALL_OK) {
        std::cerr << "Could not make live image bootable: " << ErrorMessage(status, error) << "\n";
        return EX_SOFTWARE;
    }

    if (reboot) {
        if (!android::base::SetProperty(ANDROID_RB_PROPERTY, "reboot,adb")) {
            std::cerr << "Failed to reboot automatically" << std::endl;
            return EX_SOFTWARE;
        }
    } else {
        std::cout << "Please reboot to use the GSI." << std::endl;
    }
    return 0;
}

static int Wipe(sp<IGsiService> gsid, int argc, char** /* argv */) {
    if (argc > 1) {
        std::cerr << "Unrecognized arguments to wipe." << std::endl;
        return EX_USAGE;
    }
    bool ok;
    auto status = gsid->removeGsi(&ok);
    if (!status.isOk() || !ok) {
        std::cerr << "Could not remove GSI install: " << ErrorMessage(status) << "\n";
        return EX_SOFTWARE;
    }

    bool running = false;
    if (gsid->isGsiRunning(&running).isOk() && running) {
        std::cout << "Live image install will be removed next reboot." << std::endl;
    } else {
        std::cout << "Live image install successfully removed." << std::endl;
    }
    return 0;
}

static int WipeData(sp<IGsiService> gsid, int argc, char** /* argv */) {
    if (argc > 1) {
        std::cerr << "Unrecognized arguments to wipe-data.\n";
        return EX_USAGE;
    }

    bool running;
    auto status = gsid->isGsiRunning(&running);
    if (!status.isOk()) {
        std::cerr << "error: " << status.exceptionMessage().string() << std::endl;
        return EX_SOFTWARE;
    }
    if (running) {
        std::cerr << "Cannot wipe GSI userdata while running a GSI.\n";
        return EX_USAGE;
    }

    bool installed;
    status = gsid->isGsiInstalled(&installed);
    if (!status.isOk()) {
        std::cerr << "error: " << status.exceptionMessage().string() << std::endl;
        return EX_SOFTWARE;
    }
    if (!installed) {
        std::cerr << "No GSI is installed.\n";
        return EX_USAGE;
    }

    int error;
    status = gsid->zeroPartition("userdata" + std::string(kDsuPostfix), &error);
    if (!status.isOk() || error) {
        std::cerr << "Could not wipe GSI userdata: " << ErrorMessage(status, error) << "\n";
        return EX_SOFTWARE;
    }
    return 0;
}

static int Status(sp<IGsiService> gsid, int argc, char** /* argv */) {
    if (argc > 1) {
        std::cerr << "Unrecognized arguments to status." << std::endl;
        return EX_USAGE;
    }
    bool running;
    auto status = gsid->isGsiRunning(&running);
    if (!status.isOk()) {
        std::cerr << "error: " << status.exceptionMessage().string() << std::endl;
        return EX_SOFTWARE;
    } else if (running) {
        std::cout << "running" << std::endl;
    }
    bool installed;
    status = gsid->isGsiInstalled(&installed);
    if (!status.isOk()) {
        std::cerr << "error: " << status.exceptionMessage().string() << std::endl;
        return EX_SOFTWARE;
    } else if (installed) {
        std::cout << "installed" << std::endl;
    }
    bool enabled;
    status = gsid->isGsiEnabled(&enabled);
    if (!status.isOk()) {
        std::cerr << status.exceptionMessage().string() << std::endl;
        return EX_SOFTWARE;
    } else if (running || installed) {
        std::cout << (enabled ? "enabled" : "disabled") << std::endl;
    } else {
        std::cout << "normal" << std::endl;
    }
    if (getuid() != 0) {
        return 0;
    }

    std::vector<std::string> dsu_slots;
    status = gsid->getInstalledDsuSlots(&dsu_slots);
    if (!status.isOk()) {
        std::cerr << status.exceptionMessage().string() << std::endl;
        return EX_SOFTWARE;
    }
    int n = 0;
    for (auto&& dsu_slot : dsu_slots) {
        std::cout << "[" << n++ << "] " << dsu_slot << std::endl;
        sp<IImageService> image_service = nullptr;
        status = gsid->openImageService("dsu/" + dsu_slot + "/", &image_service);
        if (!status.isOk()) {
            std::cerr << "error: " << status.exceptionMessage().string() << std::endl;
            return EX_SOFTWARE;
        }
        std::vector<std::string> images;
        status = image_service->getAllBackingImages(&images);
        if (!status.isOk()) {
            std::cerr << "error: " << status.exceptionMessage().string() << std::endl;
            return EX_SOFTWARE;
        }
        for (auto&& image : images) {
            std::cout << "installed: " << image << std::endl;
            AvbPublicKey public_key;
            int err = 0;
            status = image_service->getAvbPublicKey(image, &public_key, &err);
            std::cout << "AVB public key (sha1): ";
            if (!public_key.bytes.empty()) {
                for (auto b : public_key.sha1) {
                    std::cout << StringPrintf("%02x", b & 255);
                }
                std::cout << std::endl;
            } else {
                std::cout << "[NONE]" << std::endl;
            }
        }
    }
    return 0;
}

static int Cancel(sp<IGsiService> gsid, int /* argc */, char** /* argv */) {
    bool cancelled = false;
    auto status = gsid->cancelGsiInstall(&cancelled);
    if (!status.isOk()) {
        std::cerr << status.exceptionMessage().string() << std::endl;
        return EX_SOFTWARE;
    }
    if (!cancelled) {
        std::cout << "Fail to cancel the installation." << std::endl;
        return EX_SOFTWARE;
    }
    return 0;
}

static int Enable(sp<IGsiService> gsid, int argc, char** argv) {
    bool one_shot = false;
    std::string dsuSlot = {};
    struct option options[] = {
            {"single-boot", no_argument, nullptr, 's'},
            {"dsuslot", required_argument, nullptr, 'd'},
            {nullptr, 0, nullptr, 0},
    };
    int rv, index;
    while ((rv = getopt_long_only(argc, argv, "", options, &index)) != -1) {
        switch (rv) {
            case 's':
                one_shot = true;
                break;
            case 'd':
                dsuSlot = optarg;
                break;
            default:
                std::cerr << "Unrecognized argument to enable\n";
                return EX_USAGE;
        }
    }

    bool installed = false;
    gsid->isGsiInstalled(&installed);
    if (!installed) {
        std::cerr << "Could not find GSI install to re-enable" << std::endl;
        return EX_SOFTWARE;
    }

    bool installing = false;
    gsid->isGsiInstallInProgress(&installing);
    if (installing) {
        std::cerr << "Cannot enable or disable while an installation is in progress." << std::endl;
        return EX_SOFTWARE;
    }
    if (dsuSlot.empty()) {
        auto status = gsid->getActiveDsuSlot(&dsuSlot);
        if (!status.isOk()) {
            std::cerr << "Could not get the active DSU slot: " << ErrorMessage(status) << "\n";
            return EX_SOFTWARE;
        }
    }
    int error;
    auto status = gsid->enableGsi(one_shot, dsuSlot, &error);
    if (!status.isOk() || error != IGsiService::INSTALL_OK) {
        std::cerr << "Error re-enabling GSI: " << ErrorMessage(status, error) << "\n";
        return EX_SOFTWARE;
    }
    std::cout << "Live image install successfully enabled." << std::endl;
    return 0;
}

static int Disable(sp<IGsiService> gsid, int argc, char** /* argv */) {
    if (argc > 1) {
        std::cerr << "Unrecognized arguments to disable." << std::endl;
        return EX_USAGE;
    }

    bool installing = false;
    gsid->isGsiInstallInProgress(&installing);
    if (installing) {
        std::cerr << "Cannot enable or disable while an installation is in progress." << std::endl;
        return EX_SOFTWARE;
    }

    bool ok = false;
    gsid->disableGsi(&ok);
    if (!ok) {
        std::cerr << "Error disabling GSI" << std::endl;
        return EX_SOFTWARE;
    }
    std::cout << "Live image install successfully disabled." << std::endl;
    return 0;
}

static int usage(int /* argc */, char* argv[]) {
    fprintf(stderr,
            "%s - command-line tool for installing GSI images.\n"
            "\n"
            "Usage:\n"
            "  %s <disable|install|wipe|status> [options]\n"
            "\n"
            "  disable      Disable the currently installed GSI.\n"
            "  enable       [-s, --single-boot]\n"
            "               [-d, --dsuslot slotname]\n"
            "               Enable a previously disabled GSI.\n"
            "  install      Install a new GSI. Specify the image size with\n"
            "               --gsi-size and the desired userdata size with\n"
            "               --userdata-size (the latter defaults to 8GiB)\n"
            "               --wipe (remove old gsi userdata first)\n"
            "  wipe         Completely remove a GSI and its associated data\n"
            "  wipe-data    Ensure the GSI's userdata will be formatted\n"
            "  cancel       Cancel the installation\n"
            "  status       Show status\n",
            argv[0], argv[0]);
    return EX_USAGE;
}

int main(int argc, char** argv) {
    android::base::InitLogging(argv, android::base::StdioLogger, android::base::DefaultAborter);

    android::sp<IGsiService> service = GetGsiService();
    if (!service) {
        return EX_SOFTWARE;
    }

    if (1 >= argc) {
        std::cerr << "Expected command." << std::endl;
        return EX_USAGE;
    }

    std::string command = argv[1];

    auto iter = kCommandMap.find(command);
    if (iter == kCommandMap.end()) {
        std::cerr << "Unrecognized command: " << command << std::endl;
        return usage(argc, argv);
    }

    int rc = iter->second(service, argc - 1, argv + 1);
    return rc;
}
