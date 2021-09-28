/*
 * Copyright 2020 The Android Open Source Project
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

namespace bluetooth {
namespace bluetooth_keystore {

class BluetoothKeystoreCallbacks {
 public:
  virtual ~BluetoothKeystoreCallbacks() = default;

  /** Callback for key encrypt or remove key */
  virtual void set_encrypt_key_or_remove_key(std::string prefix,
                                             std::string encryptedString) = 0;

  /** Callback for get key. */
  virtual std::string get_key(std::string prefix) = 0;
};

class BluetoothKeystoreInterface {
 public:
  virtual ~BluetoothKeystoreInterface() = default;

  /** Register the bluetooth keystore callbacks */
  virtual void init(BluetoothKeystoreCallbacks* callbacks) = 0;

  /** Interface for key encrypt or remove key */
  virtual void set_encrypt_key_or_remove_key(std::string prefix,
                                             std::string encryptedString) = 0;

  /** Interface for get key. */
  virtual std::string get_key(std::string prefix) = 0;

  /** Interface for clear map. */
  virtual void clear_map() = 0;
};

}  // namespace bluetooth_keystore
}  // namespace bluetooth
