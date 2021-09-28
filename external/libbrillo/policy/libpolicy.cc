// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "policy/libpolicy.h"

#include <memory>

#include <base/logging.h>

#include "policy/device_policy.h"
#ifndef __ANDROID__
#include "policy/device_policy_impl.h"
#endif

namespace policy {

PolicyProvider::PolicyProvider() {
#ifndef __ANDROID__
  device_policy_ = std::make_unique<DevicePolicyImpl>();
  install_attributes_reader_ = std::make_unique<InstallAttributesReader>();
#endif
}

PolicyProvider::PolicyProvider(std::unique_ptr<DevicePolicy> device_policy)
    : device_policy_(std::move(device_policy)),
#ifdef __ANDROID__
      device_policy_is_loaded_(true) {
}
#else
      device_policy_is_loaded_(true),
      install_attributes_reader_(std::make_unique<InstallAttributesReader>()) {
}
#endif  // __ANDROID__

PolicyProvider::~PolicyProvider() {}

bool PolicyProvider::Reload() {
  if (!device_policy_)
    return false;
  device_policy_is_loaded_ = device_policy_->LoadPolicy();
  if (!device_policy_is_loaded_) {
    LOG(WARNING) << "Could not load the device policy file.";
  }
  return device_policy_is_loaded_;
}

bool PolicyProvider::device_policy_is_loaded() const {
  return device_policy_is_loaded_;
}

const DevicePolicy& PolicyProvider::GetDevicePolicy() const {
  DCHECK(device_policy_is_loaded_)
      << "Trying to get policy data but policy was not loaded!";
  return *device_policy_;
}

bool PolicyProvider::IsConsumerDevice() const {
#ifdef __ANDROID__
  return true;
#else
  if (!install_attributes_reader_->IsLocked())
    return false;

  const std::string& device_mode = install_attributes_reader_->GetAttribute(
      InstallAttributesReader::kAttrMode);
  return device_mode != InstallAttributesReader::kDeviceModeEnterprise &&
         device_mode != InstallAttributesReader::kDeviceModeEnterpriseAD;
#endif  // __ANDROID__
}

void PolicyProvider::SetDevicePolicyForTesting(
    std::unique_ptr<DevicePolicy> device_policy) {
  device_policy_ = std::move(device_policy);
  device_policy_is_loaded_ = true;
}

void PolicyProvider::SetInstallAttributesReaderForTesting(
    std::unique_ptr<InstallAttributesReader> install_attributes_reader) {
  install_attributes_reader_ = std::move(install_attributes_reader);
}

}  // namespace policy
