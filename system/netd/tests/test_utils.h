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
 * test_utils.h - miscellaneous unit test utilities.
 */

#pragma once

#include <string>
#include <vector>

int randomUid();

std::vector<std::string> runCommand(const std::string& command);

std::vector<std::string> listIpRules(const char* ipVersion);

std::vector<std::string> listIptablesRule(const char* binary, const char* chainName);

int iptablesRuleLineLength(const char* binary, const char* chainName);

bool iptablesRuleExists(const char* binary, const char* chainName, const std::string& expectedRule);

std::vector<std::string> listIpRoutes(const char* ipVersion, const char* table);

bool ipRouteExists(const char* ipVersion, const char* table, const std::string& ipRoute);
