# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Emit commands needed for Rockchip devices during OTA installation
(installing the RK30xxLoader.bin)."""

import common
import re
import os
from common import BlockDifference, EmptyImage, GetUserImage

# The joined list of user image partitions of source and target builds.
# - Items should be added to the list if new dynamic partitions are added.
# - Items should not be removed from the list even if dynamic partitions are
#   deleted. When generating an incremental OTA package, this script needs to
#   know that an image is present in source build but not in target build.
USERIMAGE_PARTITIONS = [
    "odm",
    "product",
    "system_ext",
]

def FullOTA_Assertions(info):
  ## we suggest not update the parameter.
  #parameter_bin = info.input_zip.read("parameter")
  #Install_Parameter(parameter_bin,info.input_zip,info)
  AddBootloaderAssertion(info, info.input_zip)

def IncrementalOTA_Assertions(info):
  AddBootloaderAssertion(info, info.target_zip)

def AddBootloaderAssertion(info, input_zip):
  android_info = input_zip.read("OTA/android-info.txt")
  m = re.search(r"require\s+version-bootloader\s*=\s*(\S+)", android_info)
  if m:
    bootloaders = m.group(1).split("|")
    if "*" not in bootloaders:
      info.script.AssertSomeBootloader(*bootloaders)
    info.metadata["pre-bootloader"] = m.group(1)

def Install_Parameter(parameter_bin, input_zip, info):
  try:
    print "write parameter.bin now"
    info.script.Print("Start update GptParameter...")
    common.ZipWriteStr(info.output_zip, "parameter", parameter_bin)
    info.script.WriteRawGptParameterImage()
    info.script.Print("end update parameter")
    info.script.FormatPartition("/data");
  except KeyError:
    print "no parameter.bin, ignore it."

def InstallRKLoader(loader_bin, input_zip, info):
  try:
    print "wirte RKLoader.bin now..."
    #info.script.Print("Writing rk loader bin...")
    common.ZipWriteStr(info.output_zip, "RKLoader.bin", loader_bin)
    #info.script.WriteRawLoaderImage()
  except KeyError:
    print "no RKLoader.bin, ignore it."


def InstallUboot(loader_bin, input_zip, info):
  common.ZipWriteStr(info.output_zip, "uboot.img", loader_bin)
  info.script.Print("Writing uboot loader img...")
  info.script.WriteRawImage("/uboot", "uboot.img")

def InstallTrust(trust_bin, input_zip, info):
  common.ZipWriteStr(info.output_zip, "trust.img", trust_bin)
  info.script.Print("Writing trust img...")
  info.script.WriteRawImage("/trust", "trust.img")

def InstallVbmeta(vbmeta_bin, input_zip, info):
  common.ZipWriteStr(info.output_zip, "vbmeta.img",vbmeta_bin)
  info.script.Print("Writing vbmeta img...")
  info.script.WriteRawImage("/vbmeta", "vbmeta.img")

def InstallDtbo(dtbo_bin, input_zip, info):
  common.ZipWriteStr(info.output_zip, "dtbo.img",dtbo_bin)
  info.script.Print("Writing dtbo img...")
  info.script.WriteRawImage("/dtbo", "dtbo.img")

def InstallCharge(charge_bin, input_zip, info):
  common.ZipWriteStr(info.output_zip, "charge.img", charge_bin)
  info.script.Print("Writing charge img..")
  info.script.WriteRawImage("/charge", "charge.img")

def InstallResource(resource_bin, input_zip, info):
  common.ZipWriteStr(info.output_zip, "resource.img", resource_bin)
  info.script.Print("Writing resource image..")
  info.script.WriteRawImage("/resource", "resource.img")

def InstallVendorBoot(vendor_boot_bin, input_zip, info):
  common.ZipWriteStr(info.output_zip, "vendor_boot.img",vendor_boot_bin)
  info.script.Print("Writing vendor_boot img...")
  info.script.WriteRawImage("/vendor_boot", "vendor_boot.img")

def InstallEbookLogo(logo_bin, input_zip, info):
  common.ZipWriteStr(info.output_zip, "logo.img", logo_bin)
  info.script.Print("Writing logo image..")
  info.script.WriteRawImage("/logo", "logo.img")

def FullOTA_InstallEnd(info):
  try:
    trust = info.input_zip.read("trust.img")
    print "write trust now..."
    InstallTrust(trust, info.input_zip, info)
  except KeyError:
    print "warning: no trust.img in input target_files; not flashing trust"

  try:
    uboot = info.input_zip.read("uboot.img")
    print "write uboot now..."
    InstallUboot(uboot, info.input_zip, info)
  except KeyError:
    print "warning: no uboot.img in input target_files; not flashing uboot"

  try:
    vbmeta = info.input_zip.read("IMAGES/vbmeta.img")
    print "wirte vbmeta now..."
    InstallVbmeta(vbmeta, info.input_zip, info)
  except KeyError:
    print "warning: no vbmeta.img in input target_files; not flashing vbmeta"

  try:
    dtbo = info.input_zip.read("IMAGES/dtbo.img")
    print "wirte dtbo now..."
    InstallDtbo(dtbo, info.input_zip, info)
  except KeyError:
    print "warning: no dtbo.img in input target_files; not flashing dtbo"

  try:
    charge = info.input_zip.read("charge.img")
    print "wirte charge now..."
    InstallCharge(charge, info.input_zip, info)
  except KeyError:
    # print "info: no charge img; ignore it."
    print "no charge img; ignore it."

#**************************************************************************************************
#resource package in the boot.img and recovery.img,so we suggest not to update alone resource.img
#**************************************************************************************************

  try:
    resource = info.input_zip.read("IMAGES/resource.img")
    print "wirte resource now..."
    InstallResource(resource, info.input_zip, info)
  except KeyError:
    print "info: no resource image; ignore it."

#  try:
#    loader_bin = info.input_zip.read("LOADER/RKLoader.img")
#  except KeyError:
#    # print "warning: no rk loader bin in input target_files; not flashing loader"
#    print "no rk loader bin in input target_files; not flashing loader"
#    print "to add clear misc command"
#    info.script.ClearMiscCommand()
#    return

  try:
    loader_bin = info.input_zip.read("RKLoader.bin")
    InstallRKLoader(loader_bin, info.input_zip, info)
  except KeyError:
    print "no RKLoader.bin, ignore it."

  try:
    vendor_boot = info.input_zip.read("IMAGES/vendor_boot.img")
    print "wirte vendor_boot now..."
    InstallVendorBoot(vendor_boot, info.input_zip, info)
  except KeyError:
    print "info: no vendor_boot.img in input target_files; ignore it"

  try:
    logo = info.input_zip.read("IMAGES/logo.img")
    print "wirte logo now..."
    InstallEbookLogo(logo, info.input_zip, info)
  except KeyError:
    print "info: no logo image; ignore it."

def IncrementalOTA_InstallEnd(info):
  try:
    trust_target = info.target_zip.read("trust.img")
  except KeyError:
    trust_target = None

  try:
    trust_source = info.source_zip.read("trust.img")
  except KeyError:
    trust_source = None

  if (trust_target != None) and (trust_target != trust_source):
    print "write trust now..."
    InstallTrust(trust_target, info.target_zip, info)
  else:
    print "trust unchanged; skipping"

  try:
    vbmeta_target = info.target_zip.read("IMAGES/vbmeta.img")
  except KeyError:
    vbmeta_target = None

  try:
    vbmeta_source = info.source_zip.read("IMAGES/vbmeta.img")
  except KeyError:
    vbmeta_source = None

  if (vbmeta_target != None) and (vbmeta_target != vbmeta_source):
    print "write vbmeta now..."
    InstallVbmeta(vbmeta_target, info.target_zip, info)
  else:
    print "vbmeta unchanged; skipping"

  try:
    dtbo_target = info.target_zip.read("IMAGES/dtbo.img")
  except KeyError:
    dtbo_target = None

  try:
    dtbo_source = info.source_zip.read("IMAGES/dtbo.img")
  except KeyError:
    dtbo_source = None

  if (dtbo_target != None) and (dtbo_target != dtbo_source):
    print "write dtbo now..."
    InstallDtbo(dtbo_target, info.target_zip, info)
  else:
    print "dtbo unchanged; skipping"

  try:
    loader_uboot_target = info.target_zip.read("uboot.img")
  except KeyError:
    loader_uboot_target = None

  try:
    loader_uboot_source = info.source_zip.read("uboot.img")
  except KeyError:
    loader_uboot_source = None

  if (loader_uboot_target != None) and (loader_uboot_target != loader_uboot_source):
    print "write uboot now..."
    InstallUboot(loader_uboot_target, info.target_zip, info)
  else:
    print "uboot unchanged; skipping"

  try:
    charge_target = info.target_zip.read("charge.img")
  except KeyError:
    charge_target = None

  try:
    charge_source = info.source_zip.read("charge.img")
  except KeyError:
    charge_source = None

  if (charge_target != None) and (charge_target != charge_source):
    print "write charge now..."
    InstallCharge(charge_target, info.target_zip, info)
  else:
    print "charge unchanged; skipping"

#**************************************************************************************************
#resource package in the boot.img and recovery.img,so we suggest not to update alone resource.img
#**************************************************************************************************
  try:
    resource_target = info.target_zip.read("IMAGES/resource.img")
  except KeyError:
    resource_target = None

  try:
    resource_source = info.source_zip.read("IMAGES/resource.img")
  except KeyError:
    resource_source = None

  if (resource_target != None) and (resource_target != resource_source):
    print "write resource now..."
    InstallResource(resource_target, info.target_zip, info)
  else:
    print "resource unchanged; skipping"

  try:
    target_loader = info.target_zip.read("RKLoader.bin")
  except KeyError:
    print "warning: rk loader bin missing from target; not flashing loader"
    target_loader = None

  try:
    source_loader = info.source_zip.read("RKLoader.bin")
  except KeyError:
    print "warning: rk loader bin missing from source; not flashing loader"
    source_loader = None

  if (target_loader != None) and (source_loader != target_loader):
    print "write loader now..."
    InstallRKLoader(target_loader, info.target_zip, info)
  else:
    print "loader unchanged; skipping"

  try:
    vendor_boot_target = info.target_zip.read("IMAGES/vendor_boot.img")
  except KeyError:
    vendor_boot_target = None

  try:
    vendor_boot_source = info.source_zip.read("IMAGES/vendor_boot.img")
  except KeyError:
    vendor_boot_source = None

  if (vendor_boot_target != None) and (vendor_boot_target != vbmeta_source):
    print "write vendor_boot now..."
    InstallVendorBoot(vendor_boot_target, info.target_zip, info)
  else:
    print "vendor_boot unchanged; skipping"

  try:
    logo_target = info.target_zip.read("IMAGES/logo.img")
  except KeyError:
    logo_target = None

  try:
    logo_source = info.source_zip.read("IMAGES/logo.img")
  except KeyError:
    logo_source = None

  if (logo_target != None) and (logo_target != logo_source):
    print "write logo now..."
    InstallEbookLogo(logo_target, info.target_zip, info)
  else:
    print "logo unchanged; skipping"

def GetUserImages(input_tmp, input_zip):
  return {partition: GetUserImage(partition, input_tmp, input_zip)
          for partition in USERIMAGE_PARTITIONS
          if os.path.exists(os.path.join(input_tmp,
                                         "IMAGES", partition + ".img"))}

def FullOTA_GetBlockDifferences(info):
  images = GetUserImages(info.input_tmp, info.input_zip)
  return [BlockDifference(partition, image)
          for partition, image in images.items()]

def IncrementalOTA_GetBlockDifferences(info):
  source_images = GetUserImages(info.source_tmp, info.source_zip)
  target_images = GetUserImages(info.target_tmp, info.target_zip)

  # Use EmptyImage() as a placeholder for partitions that will be deleted.
  for partition in source_images:
    target_images.setdefault(partition, EmptyImage())

  # Use source_images.get() because new partitions are not in source_images.
  return [BlockDifference(partition, target_image, source_images.get(partition))
          for partition, target_image in target_images.items()]

