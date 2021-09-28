/******************************************************************************
 *
 *  Copyright 2018 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "ese_config.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include <config.h>

using namespace ::std;
using namespace ::android::base;

namespace {

std::string findConfigPath() {
  const vector<string> search_path = {"/odm/etc/", "/vendor/etc/", "/etc/"};
  const string file_name = "libese-nxp.conf";

  for (string path : search_path) {
    path.append(file_name);
    struct stat file_stat;
    if (stat(path.c_str(), &file_stat) != 0) continue;
    if (S_ISREG(file_stat.st_mode)) return path;
  }
  return "";
}

}  // namespace

EseConfig::EseConfig() {
  string config_path = findConfigPath();
  CHECK(config_path != "");
  config_.parseFromFile(config_path);
}

EseConfig& EseConfig::getInstance() {
  static EseConfig theInstance;
  return theInstance;
}

bool EseConfig::hasKey(const std::string& key) {
  return getInstance().config_.hasKey(key);
}

std::string EseConfig::getString(const std::string& key) {
  return getInstance().config_.getString(key);
}

std::string EseConfig::getString(const std::string& key,
                                 std::string default_value) {
  if (hasKey(key)) return getString(key);
  return default_value;
}

unsigned EseConfig::getUnsigned(const std::string& key) {
  return getInstance().config_.getUnsigned(key);
}

unsigned EseConfig::getUnsigned(const std::string& key,
                                unsigned default_value) {
  if (hasKey(key)) return getUnsigned(key);
  return default_value;
}

std::vector<uint8_t> EseConfig::getBytes(const std::string& key) {
  return getInstance().config_.getBytes(key);
}

void EseConfig::clear() { getInstance().config_.clear(); }
