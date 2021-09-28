// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libinstallattributes.h"

#include <base/files/file_util.h>
#include <base/logging.h>

#include "bindings/install_attributes.pb.h"

namespace {

// Written by cryptohome or by lockbox-cache after signature verification and
// thus guaranteed to be unadulterated.
const char kInstallAttributesPath[] = "/run/lockbox/install_attributes.pb";

}  // namespace

// The source of truth for these constants is Chromium
// //chrome/browser/chromeos/settings/install_attributes.cc.
const char InstallAttributesReader::kAttrMode[] = "enterprise.mode";
const char InstallAttributesReader::kDeviceModeConsumer[] = "consumer";
const char InstallAttributesReader::kDeviceModeEnterprise[] = "enterprise";
const char InstallAttributesReader::kDeviceModeEnterpriseAD[] = "enterprise_ad";
const char InstallAttributesReader::kDeviceModeLegacyRetail[] = "kiosk";
const char InstallAttributesReader::kDeviceModeConsumerKiosk[] =
    "consumer_kiosk";

InstallAttributesReader::InstallAttributesReader()
    : install_attributes_path_(kInstallAttributesPath) {
}

InstallAttributesReader::~InstallAttributesReader() {
}

const std::string& InstallAttributesReader::GetAttribute(
    const std::string& key) {
  // By its very nature of immutable attributes, once read successfully the
  // attributes can never change and thus never need reloading.
  if (!initialized_) {
    TryToLoad();
  }

  const auto entry = attributes_.find(key);
  if (entry == attributes_.end()) {
    return empty_string_;
  }
  return entry->second;
}

bool InstallAttributesReader::IsLocked() {
  if (!initialized_) {
    TryToLoad();
  }
  return initialized_;
}

void InstallAttributesReader::TryToLoad() {
  std::string contents;
  if (!base::ReadFileToString(install_attributes_path_, &contents)) {
    // May fail during OOBE or early in the boot process.
    return;
  }

  // Parse errors are unrecoverable (lockbox does atomic write), thus mark as
  // inititialized already before checking for parse errors.
  initialized_ = true;

  cryptohome::SerializedInstallAttributes install_attributes;
  if (!install_attributes.ParseFromString(contents)) {
    LOG(ERROR) << "Can't parse install attributes.";
    return;
  }

  for (int i = 0; i < install_attributes.attributes_size(); ++i) {
    const cryptohome::SerializedInstallAttributes_Attribute& attribute =
        install_attributes.attributes(i);
    // Cast value to C string and back to remove trailing zero.
    attributes_[attribute.name()] = std::string(attribute.value().c_str());
  }
}
