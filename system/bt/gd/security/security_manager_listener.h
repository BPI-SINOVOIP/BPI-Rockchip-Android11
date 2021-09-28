/*
 *
 *  Copyright 2019 The Android Open Source Project
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#pragma once

#include "common/callback.h"

namespace bluetooth {
namespace security {

/**
 * Callback interface from SecurityManager.
 */
class ISecurityManagerListener {
 public:
  virtual ~ISecurityManagerListener() = 0;

  /**
   * Called when a device is successfully bonded.
   *
   * @param address of the newly bonded device
   */
  virtual void OnDeviceBonded(bluetooth::hci::AddressWithType device) = 0;

  /**
   * Called when a device is successfully un-bonded.
   *
   * @param address of device that is no longer bonded
   */
  virtual void OnDeviceUnbonded(bluetooth::hci::AddressWithType device) = 0;

  /**
   * Called as a result of a failure during the bonding process.
   *
   * @param address of the device that failed to bond
   */
  virtual void OnDeviceBondFailed(bluetooth::hci::AddressWithType device) = 0;
};

}  // namespace security
}  // namespace bluetooth
