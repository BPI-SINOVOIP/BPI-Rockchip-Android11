/*
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

#include "wifi_command.h"

#include "../bridge.h"
#include "../utils.h"

#include <cutils/properties.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static const char kHostApdStubFile[] = "/vendor/etc/simulated_hostapd.conf";
static const char kHostApdConfFile[] = "/data/vendor/wifi/hostapd/hostapd.conf";

static const char kControlRestartProperty[] = "ctl.restart";
static const char kHostApdServiceName[] = "emu_hostapd";

static const char kIfNamePrefix[] = "wlan1_";

class File {
public:
    explicit File(FILE* file) : mFile(file) {}
    ~File() {
        if (mFile) {
            fclose(mFile);
        }
    }

    FILE* get() { return mFile; }

    bool operator!() const { return mFile == nullptr; }
private:
    FILE* mFile;
};

class Fd {
public:
    explicit Fd(int fd) : mFd(fd) {}
    ~Fd() {
        if (mFd != -1) {
            ::close(mFd);
            mFd = -1;
        }
    }

    int get() const { return mFd; }

private:
    int mFd;
};

WifiCommand::WifiCommand(Bridge& bridge)
    : mBridge(bridge)
    , mLowestInterfaceNumber(1) {
    readConfig();
}

Result WifiCommand::onCommand(const char* /*command*/, const char* args) {
    const char* divider = ::strchr(args, ' ');
    if (divider == nullptr) {
        // Unknown command, every command needs an argument
        return Result::error("Invalid wifi command '%s'", args);
    }

    std::string subCommand(args, divider);
    if (subCommand.empty()) {
        return Result::error("Empty wifi command");
    }

    std::vector<std::string> subArgs = explode(divider + 1, ' ');
    if (subArgs.empty()) {
        // All of these commands require sub arguments
        return Result::error("Missing argument to command '%s'",
                             subCommand.c_str());
    }

    if (subCommand == "add") {
        return onAdd(subArgs);
    } else if (subCommand == "block") {
        return onBlock(subArgs);
    } else if (subCommand == "unblock") {
        return onUnblock(subArgs);
    } else {
        return Result::error("Unknown wifi command '%s'", subCommand.c_str());
    }
}

void WifiCommand::readConfig() {
}

Result WifiCommand::writeConfig() {
    File in(fopen(kHostApdStubFile, "r"));
    if (!in) {
        return Result::error("Config failure: could not open template: %s",
                             strerror(errno));
    }

    File out(fopen(kHostApdConfFile, "w"));
    if (!out) {
        return Result::error("Config failure: could not open target: %s",
                             strerror(errno));
    }

    char buffer[32768];
    while (!feof(in.get())) {
        size_t bytesRead = fread(buffer, 1, sizeof(buffer), in.get());
        if (bytesRead != sizeof(buffer) && ferror(in.get())) {
            return Result::error("Config failure: Error reading template: %s",
                                 strerror(errno));
        }

        size_t bytesWritten = fwrite(buffer, 1, bytesRead, out.get());
        if (bytesWritten != bytesRead) {
            return Result::error("Config failure: Error writing target: %s",
                                 strerror(errno));
        }
    }
    fprintf(out.get(), "\n\n");

    for (const auto& ap : mAccessPoints) {
        fprintf(out.get(), "bss=%s\n", ap.second.ifName.c_str());
        fprintf(out.get(), "ssid=%s\n", ap.second.ssid.c_str());
        if (!ap.second.password.empty()) {
            fprintf(out.get(), "wpa=2\n");
            fprintf(out.get(), "wpa_key_mgmt=WPA-PSK\n");
            fprintf(out.get(), "rsn_pairwise=CCMP\n");
            fprintf(out.get(), "wpa_passphrase=%s\n", ap.second.password.c_str());
        }
        fprintf(out.get(), "\n");
    }
    return Result::success();
}

Result WifiCommand::triggerHostApd() {
    property_set(kControlRestartProperty, kHostApdServiceName);
    return Result::success();
}

Result WifiCommand::onAdd(const std::vector<std::string>& arguments) {
    AccessPoint& ap = mAccessPoints[arguments[0]];
    ap.ssid = arguments[0];
    if (arguments.size() > 1) {
        ap.password = arguments[1];
    } else {
        ap.password.clear();
    }
    if (ap.ifName.empty()) {
        char buffer[sizeof(kIfNamePrefix) + 10];
        while (true) {
            snprintf(buffer, sizeof(buffer), "%s%d",
                     kIfNamePrefix, mLowestInterfaceNumber);
            ap.ifName = buffer;
            auto usedInterface = mUsedInterfaces.find(ap.ifName);
            if (usedInterface == mUsedInterfaces.end()) {
                // This interface is available, use it
                ++mLowestInterfaceNumber;
                mUsedInterfaces.insert(ap.ifName);
                break;
            }
            // The interface name was alread in use, try the next one
            ++mLowestInterfaceNumber;
        }
    }
    Result res = writeConfig();
    if (!res) {
        return res;
    }
    return triggerHostApd();
}

Result WifiCommand::onBlock(const std::vector<std::string>& arguments) {
    auto interface = mAccessPoints.find(arguments[0]);
    if (interface == mAccessPoints.end()) {
        return Result::error("Unknown SSID '%s'", arguments[0].c_str());
    }
    interface->second.blocked = true;
    return mBridge.removeInterface(interface->second.ifName.c_str());
}

Result WifiCommand::onUnblock(const std::vector<std::string>& arguments) {
    auto interface = mAccessPoints.find(arguments[0]);
    if (interface == mAccessPoints.end()) {
        return Result::error("Unknown SSID '%s'", arguments[0].c_str());
    }
    interface->second.blocked = false;
    return mBridge.addInterface(interface->second.ifName.c_str());
}

