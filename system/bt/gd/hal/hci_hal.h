/*
 * Copyright 2019 The Android Open Source Project
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

#pragma once

#include <vector>

#include "module.h"

namespace bluetooth {
namespace hal {

using HciPacket = std::vector<uint8_t>;

enum class Status : int32_t { SUCCESS, TRANSPORT_ERROR, INITIALIZATION_ERROR, UNKNOWN };

// Mirrors hardware/interfaces/bluetooth/1.0/IBluetoothHciCallbacks.hal in Android, but moved initializationComplete
// callback to BluetoothInitializationCompleteCallback

// The interface from the Bluetooth Controller to the stack
class HciHalCallbacks {
 public:
  virtual ~HciHalCallbacks() = default;

  // This function is invoked when an HCI event is received from the
  // Bluetooth controller to be forwarded to the Bluetooth stack
  // @param event is the HCI event to be sent to the Bluetooth stack
  virtual void hciEventReceived(HciPacket event) = 0;

  // Send an ACL data packet form the controller to the host
  // @param data the ACL HCI packet to be passed to the host stack
  virtual void aclDataReceived(HciPacket data) = 0;

  // Send a SCO data packet form the controller to the host
  // @param data the SCO HCI packet to be passed to the host stack
  virtual void scoDataReceived(HciPacket data) = 0;
};

// Mirrors hardware/interfaces/bluetooth/1.0/IBluetoothHci.hal in Android
// The Host Controller Interface (HCI) is the layer defined by the Bluetooth
// specification between the software that runs on the host and the Bluetooth
// controller chip. This boundary is the natural choice for a Hardware
// Abstraction Layer (HAL). Dealing only in HCI packets and events simplifies
// the stack and abstracts away power management, initialization, and other
// implementation-specific details related to the hardware.
class HciHal : public ::bluetooth::Module {
 public:
  static const ModuleFactory Factory;

  virtual ~HciHal() = default;

  // Register the callback for incoming packets. All incoming packets are dropped before
  // this callback is registered. Callback can only be registered once.
  //
  // @param callback implements BluetoothHciHalCallbacks which will
  //    receive callbacks when incoming HCI packets are received
  //    from the controller to be sent to the host.
  virtual void registerIncomingPacketCallback(HciHalCallbacks* callback) = 0;

  // Unregister the callback for incoming packets. Drop all further incoming packets.
  virtual void unregisterIncomingPacketCallback() = 0;

  // Send an HCI command (as specified in the Bluetooth Specification
  // V4.2, Vol 2, Part 5, Section 5.4.1) to the Bluetooth controller.
  // Commands must be executed in order.
  virtual void sendHciCommand(HciPacket command) = 0;

  // Send an HCI ACL data packet (as specified in the Bluetooth Specification
  // V4.2, Vol 2, Part 5, Section 5.4.2) to the Bluetooth controller.
  // Packets must be processed in order.
  virtual void sendAclData(HciPacket data) = 0;

  // Send an SCO data packet (as specified in the Bluetooth Specification
  // V4.2, Vol 2, Part 5, Section 5.4.3) to the Bluetooth controller.
  // Packets must be processed in order.
  virtual void sendScoData(HciPacket data) = 0;
};

}  // namespace hal
}  // namespace bluetooth
