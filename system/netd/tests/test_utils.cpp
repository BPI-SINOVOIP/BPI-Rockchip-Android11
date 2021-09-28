/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * test_utils.cpp - miscellaneous unit test utilities.
 */

#include <cstdio>
#include <string>
#include <vector>

#include <android-base/stringprintf.h>

#include "test_utils.h"

#define IP_PATH "/system/bin/ip"

using android::base::StringPrintf;

int randomUid() {
    // Pick a random UID consisting of:
    // - Random user profile (0 - 6)
    // - Random app ID starting from 12000 (FIRST_APPLICATION_UID + 2000). This ensures no conflicts
    //   with existing app UIDs unless the user has installed more than 2000 apps, and is still less
    //   than LAST_APPLICATION_UID (19999).
    return 100000 * arc4random_uniform(7) + 12000 + arc4random_uniform(3000);
}

std::vector<std::string> runCommand(const std::string& command) {
    std::vector<std::string> lines;
    FILE* f = popen(command.c_str(), "r");  // NOLINT(cert-env33-c)
    if (f == nullptr) {
        perror("popen");
        return lines;
    }

    char* line = nullptr;
    size_t bufsize = 0;
    ssize_t linelen = 0;
    while ((linelen = getline(&line, &bufsize, f)) >= 0) {
        lines.push_back(std::string(line, linelen));
        free(line);
        line = nullptr;
    }

    pclose(f);
    return lines;
}

std::vector<std::string> listIpRules(const char* ipVersion) {
    std::string command = StringPrintf("%s %s rule list", IP_PATH, ipVersion);
    return runCommand(command);
}

std::vector<std::string> listIptablesRule(const char* binary, const char* chainName) {
    std::string command = StringPrintf("%s -w -n -L %s", binary, chainName);
    return runCommand(command);
}

int iptablesRuleLineLength(const char* binary, const char* chainName) {
    return listIptablesRule(binary, chainName).size();
}

bool iptablesRuleExists(const char* binary, const char* chainName,
                        const std::string& expectedRule) {
    std::vector<std::string> rules = listIptablesRule(binary, chainName);
    for (const auto& rule : rules) {
        if (rule.find(expectedRule) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> listIpRoutes(const char* ipVersion, const char* table) {
    std::string command = StringPrintf("%s %s route ls table %s", IP_PATH, ipVersion, table);
    return runCommand(command);
}

bool ipRouteExists(const char* ipVersion, const char* table, const std::string& ipRoute) {
    std::vector<std::string> routes = listIpRoutes(ipVersion, table);
    for (const auto& route : routes) {
        if (route.find(ipRoute) != std::string::npos) {
            return true;
        }
    }
    return false;
}
