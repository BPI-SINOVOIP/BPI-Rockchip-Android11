// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_install_attributes_reader.h"

MockInstallAttributesReader::MockInstallAttributesReader(
    const cryptohome::SerializedInstallAttributes& install_attributes) {
  for (int i = 0; i < install_attributes.attributes_size(); ++i) {
    const cryptohome::SerializedInstallAttributes_Attribute& attribute =
        install_attributes.attributes(i);
    // Cast value to C string and back to remove trailing zero.
    attributes_[attribute.name()] = std::string(attribute.value().c_str());
  }
  initialized_ = true;
}

MockInstallAttributesReader::MockInstallAttributesReader(
    const std::string& device_mode, bool initialized) {
  attributes_[kAttrMode] = device_mode;
  initialized_ = initialized;
}
