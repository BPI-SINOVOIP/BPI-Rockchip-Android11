// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_POLICY_LIBPOLICY_H_
#define LIBBRILLO_POLICY_LIBPOLICY_H_

#include <memory>
#include <string>

#include <base/macros.h>

#include "install_attributes/libinstallattributes.h"

#pragma GCC visibility push(default)

namespace policy {

class DevicePolicy;

// This class holds device settings that are to be enforced across all users.
//
// If there is a policy on disk at creation time, we will load it at verify
// its signature.
class PolicyProvider {
 public:
  // The default constructor does not load policy.
  PolicyProvider();
  virtual ~PolicyProvider();

  // Constructor for tests only!
  explicit PolicyProvider(std::unique_ptr<DevicePolicy> device_policy);

  // This function will ensure the freshness of the contents that the getters
  // are delivering. Normally contents are cached to prevent unnecessary load.
  virtual bool Reload();

  virtual bool device_policy_is_loaded() const;

  // Returns a value from the device policy cache.
  virtual const DevicePolicy& GetDevicePolicy() const;

  // Returns true if the device is not an enterprise enrolled device, so it
  // won't have device policy before the next powerwash. Returns false if device
  // is still in OOBE (so device mode is not determined yet).
  virtual bool IsConsumerDevice() const;

  void SetDevicePolicyForTesting(
      std::unique_ptr<DevicePolicy> device_policy);
  void SetInstallAttributesReaderForTesting(
      std::unique_ptr<InstallAttributesReader> install_attributes_reader);

 private:
  std::unique_ptr<DevicePolicy> device_policy_;
  bool device_policy_is_loaded_ = false;
  std::unique_ptr<InstallAttributesReader> install_attributes_reader_;

  DISALLOW_COPY_AND_ASSIGN(PolicyProvider);
};
}  // namespace policy

#pragma GCC visibility pop

#endif  // LIBBRILLO_POLICY_LIBPOLICY_H_
