// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBBRILLO_INSTALL_ATTRIBUTES_LIBINSTALLATTRIBUTES_H_
#define LIBBRILLO_INSTALL_ATTRIBUTES_LIBINSTALLATTRIBUTES_H_

#include <map>
#include <string>

#include <base/files/file_path.h>
#include <brillo/brillo_export.h>

// Simple caching reader for the (verified) install attributes, a TPM-backed
// write once read many store.  Install attributes may be written exactly once
// by a single, atomic write-and-lock operation encompassing zero or more
// attributes.  Once locked, install attributes cannot be re-written unless TPM
// is reset (eg. by powerwashing the device).
class BRILLO_EXPORT InstallAttributesReader {
 public:
  static const char kAttrMode[];

  // Constants for the possible device modes.
  static const char kDeviceModeConsumer[];
  static const char kDeviceModeEnterprise[];
  static const char kDeviceModeEnterpriseAD[];
  static const char kDeviceModeLegacyRetail[];
  static const char kDeviceModeConsumerKiosk[];

  InstallAttributesReader();
  virtual ~InstallAttributesReader();

  // Try to load install attributes (unless cached already) and return the
  // attribute for |key| or an empty string in case |key| doesn't exist or in
  // case install attributes couldn't (yet) be loaded.  The latter is expected
  // during OOBE (install attributes haven't yet been finalized) or early in the
  // boot sequence (install attributes haven't yet been verified).
  const std::string& GetAttribute(const std::string& key);

  // Try to load install attributes (unless cached already) and return whether
  // they have yet been written-and-locked.
  bool IsLocked();

 protected:
  // Attributes cache.
  std::map<std::string, std::string> attributes_;

  // Path to the *verified* install attributes file on disk.
  base::FilePath install_attributes_path_;

  // Whether install attributes have been read successfully.  Reading a file
  // containing an empty attributes proto indicates consumer mode and counts as
  // successful, too.
  bool initialized_ = false;

private:
  // Try to load the verified install attributes from disk.  This is expected to
  // fail when install attributes haven't yet been finalized (OOBE) or verified
  // (early in the boot sequence).
  void TryToLoad();

  // Empty string to return on error.
  std::string empty_string_;
};

#endif  // LIBBRILLO_LIBINSTALLATTRIBUTES_H_
