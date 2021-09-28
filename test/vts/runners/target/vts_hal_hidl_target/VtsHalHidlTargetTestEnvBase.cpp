/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "VtsHalHidlTargetTestEnvBase"

#include "VtsHalHidlTargetTestEnvBase.h"

#include <iostream>
#include <string>

#include <hidl-util/FqInstance.h>

static const std::string kListFlag = "--list_registered_services";
static const std::string kServiceInstanceFlag = "--hal_service_instance=";

using namespace std;

namespace testing {

void VtsHalHidlTargetTestEnvBase::SetUp() {
  if (!inited_) {
    cerr << "Environment not inited, did you forget to call init()?" << endl;
    exit(-1);
  }
  // Register services used in the test.
  registerTestServices();
  // For a dummy run which just print the registered hal services.
  if (listService_) {
    listRegisteredServices();
    exit(0);
  }
  // Call the customized setup process.
  HidlSetUp();
}

void VtsHalHidlTargetTestEnvBase::TearDown() {
  // Call the customized teardown process.
  HidlTearDown();
}

void VtsHalHidlTargetTestEnvBase::init(int* argc, char** argv) {
  if (inited_) return;
  for (int i = 1; i < *argc; i++) {
    if (parseVtsTestOption(argv[i])) {
      // Shift the remainder of the argv list left by one.
      for (int j = i; j != *argc; j++) {
        argv[j] = argv[j + 1];
      }

      // Decrements the argument count.
      (*argc)--;

      // We also need to decrement the iterator as we just removed an element.
      i--;
    }
  }
  inited_ = true;
}

bool VtsHalHidlTargetTestEnvBase::parseVtsTestOption(const char* arg) {
  // arg must not be NULL.
  if (arg == NULL) return false;

  if (arg == kListFlag) {
    listService_ = true;
    return true;
  }

  if (string(arg).find(kServiceInstanceFlag) == 0) {
    // value is the part after "--hal_service_instance="
    string value = string(arg).substr(kServiceInstanceFlag.length());
    addHalServiceInstance(value);
    return true;
  }
  return false;
}

void VtsHalHidlTargetTestEnvBase::addHalServiceInstance(
    const string& halServiceInstance) {
  // hal_service_instance follows the format:
  // package@version::interface/instance e.g.:
  // android.hardware.vibrator@1.0::IVibrator/default
  if (!isValidInstance(halServiceInstance)) {
    cerr << "Input instance " << halServiceInstance
         << "does not confirm to the HAL instance format. "
         << "Expect format: package@version::interface/instance." << endl;
    exit(-1);
  }
  string halName = halServiceInstance.substr(0, halServiceInstance.find('/'));
  string instanceName =
      halServiceInstance.substr(halServiceInstance.find('/') + 1);
  // Fail the process if trying to pass multiple service names for the same
  // service instance.
  if (halServiceInstances_.find(halName) != halServiceInstances_.end()) {
    cerr << "Existing instance " << halName << "with name "
         << halServiceInstances_[halName] << endl;
    exit(-1);
  }
  halServiceInstances_[halName] = instanceName;
}

string VtsHalHidlTargetTestEnvBase::getServiceName(const string& instanceName,
                                                   const string& defaultName) {
  if (halServiceInstances_.find(instanceName) != halServiceInstances_.end()) {
    return halServiceInstances_[instanceName];
  }
  // Could not find the instance.
  cerr << "Does not find service name for " << instanceName
       << " using default name: " << defaultName << endl;
  return defaultName;
}

void VtsHalHidlTargetTestEnvBase::registerTestService(const string& FQName) {
  registeredHalServices_.insert(FQName);
}

void VtsHalHidlTargetTestEnvBase::listRegisteredServices() {
  for (const string& service : registeredHalServices_) {
    cout << "hal_service: " << service << endl;
  }
  cout << "service_comb_mode: " << mode_ << endl;
}

bool VtsHalHidlTargetTestEnvBase::isValidInstance(
    const string& halServiceInstance) {
  ::android::FqInstance fqInstance;
  return (fqInstance.setTo(halServiceInstance) && fqInstance.hasPackage() &&
          fqInstance.hasVersion() && fqInstance.hasInterface() &&
          fqInstance.hasInstance());
}

}  // namespace testing
