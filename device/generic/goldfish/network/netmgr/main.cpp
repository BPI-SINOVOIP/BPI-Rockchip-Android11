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

#include "bridge.h"
#include "bridge_builder.h"
#include "commander.h"
#include "commands/wifi_command.h"
#include "log.h"
#include "monitor.h"
#include "poller.h"
#include "utils.h"
#include "wifi_forwarder.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#include <cutils/properties.h>

#include <functional>

static const char kBridgeName[] = "br0";
static const char kNetworkBridgedProperty[] = "vendor.network.bridged";

static void usage(const char* name) {
    LOGE("Usage: %s --if-prefix <prefix> --bridge <if1,if2,...>", name);
    LOGE("  <prefix> indicates the name of network interfaces to configure.");
    LOGE("  <ip/mask> is the base IP address to assign to the first interface");
    LOGE("  and mask indicates the netmask and broadcast to set.");
    LOGE("  Additionally mask is used to determine the address");
    LOGE("  for the second interface by skipping ahead one subnet");
    LOGE("  and the size of the subnet is indicated by <mask>");
}

static Result addBridgeInterfaces(Bridge& bridge, const char* interfaces) {
    std::vector<std::string> ifNames = explode(interfaces, ',');

    for (const auto& ifName : ifNames) {
        Result res = bridge.addInterface(ifName);
        if (!res) {
            return res;
        }
    }
    return Result::success();
}

int main(int argc, char* argv[]) {
    const char* interfacePrefix = nullptr;
    const char* bridgeInterfaces = nullptr;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--if-prefix") == 0 && i + 1 < argc) {
            interfacePrefix = argv[++i];
        } else if (strcmp(argv[i], "--bridge") == 0 && i + 1 < argc) {
            bridgeInterfaces = argv[++i];
        } else {
            LOGE("Unknown parameter '%s'", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    if (interfacePrefix == nullptr) {
        LOGE("Missing parameter --if-prefix");
    }
    if (bridgeInterfaces == nullptr) {
        LOGE("Missing parameter --bridge");
    }
    if (interfacePrefix == nullptr || bridgeInterfaces == nullptr) {
        usage(argv[0]);
        return 1;
    }

    Bridge bridge(kBridgeName);
    Result res = bridge.init();
    if (!res) {
        LOGE("%s", res.c_str());
        return 1;
    }
    res = addBridgeInterfaces(bridge, bridgeInterfaces);
    if (!res) {
        LOGE("%s", res.c_str());
        return 1;
    }

    BridgeBuilder bridgeBuilder(bridge, interfacePrefix);

    property_set(kNetworkBridgedProperty, "1");

    Monitor monitor;

    monitor.setOnInterfaceState(std::bind(&BridgeBuilder::onInterfaceState,
                                          &bridgeBuilder,
                                          std::placeholders::_1,
                                          std::placeholders::_2,
                                          std::placeholders::_3));

    res = monitor.init();
    if (!res) {
        LOGE("%s", res.c_str());
        return 1;
    }

    Commander commander;
    res = commander.init();
    if (!res) {
        LOGE("%s", res.c_str());
        return 1;
    }

    WifiCommand wifiCommand(bridge);
    commander.registerCommand("wifi", &wifiCommand);

    Poller poller;
    poller.addPollable(&monitor);
    poller.addPollable(&commander);

    return poller.run();
}

