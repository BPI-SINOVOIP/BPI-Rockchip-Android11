/*
 * Copyright (C) 2020 Google LLC
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
package com.google.carrier;

import static java.nio.charset.StandardCharsets.UTF_8;

import com.beust.jcommander.JCommander;
import com.beust.jcommander.Parameter;
import com.beust.jcommander.Parameters;
import com.google.carrier.CarrierSettings;
import com.google.carrier.MultiCarrierSettings;
import com.google.protobuf.ExtensionRegistry;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Paths;

/**
 * This command line tool generates device-specific settings from device overlay and base settings.
 */
@Parameters(separators = "=")
public class GenDeviceSettings {
  @Parameter(
      names = "--version_offset",
      description =
          "The value to be added to file version, used to differentiate releases/branches.")
  private long versionOffset = 0L;

  @Parameter(names = "--device_overlay", description = "The input device override textpb file.")
  private String deviceFileName = "/tmp/device/muskie.textpb";

  @Parameter(names = "--base_setting_dir", description = "The path to input settings directory.")
  private String baseSettingDirName = "/tmp/setting";

  @Parameter(
      names = "--others_file",
      description = "The file name of others carrier settings in the input directory.")
  private String othersFileName = "others.textpb";

  @Parameter(
      names = "--device_setting_dir",
      description = "The path to putput <deice>_settings directory.")
  private String deviceSettingDirName = "/tmp/muskie_setting";

  @Parameter(
      names = "--with_version_number",
      description = "Encode version number into output pb filename.")
  private boolean versionInFileName = false;

  @Parameter(names = "--with_device_name", description = "Encode device name into output filename.")
  private String deviceInFileName = "";

  private static final String PB_SUFFIX = ".pb";
  private static final String TEXT_PB_SUFFIX = ".textpb";

  private static final ExtensionRegistry registry = ExtensionRegistry.newInstance();

  public static void main(String[] args) throws IOException {
    GenDeviceSettings generator = new GenDeviceSettings();
    new JCommander(generator, args);
    generator.generate();
  }

  private void generate() throws IOException {
    // Load device overlay
    MultiCarrierSettings deviceOverlay = null;
    try (BufferedReader br = Files.newBufferedReader(Paths.get(deviceFileName), UTF_8)) {
      MultiCarrierSettings.Builder builder = MultiCarrierSettings.newBuilder();
      TextFormat.getParser().merge(br, registry, builder);
      deviceOverlay = builder.build();
    }

    // Create output settings directory if not exist.
    File deviceSettingDir = new File(deviceSettingDirName);
    if (!deviceSettingDir.exists()) {
      deviceSettingDir.mkdirs();
    }

    // For each carrier (and others) in baseSettingDir, find its overlay and apply.
    File baseSettingDir = new File(baseSettingDirName);
    for (String childName : baseSettingDir.list((dir, name) -> name.endsWith(TEXT_PB_SUFFIX))) {
      System.out.println("Processing " + childName);

      File baseSettingFile = new File(baseSettingDir, childName);

      Message generatedMessage = null;
      long version = 0L;

      if (othersFileName.equals(childName)) {

        // Load others setting
        MultiCarrierSettings.Builder othersSetting = null;
        try (BufferedReader br = Files.newBufferedReader(baseSettingFile.toPath(), UTF_8)) {
          MultiCarrierSettings.Builder builder = MultiCarrierSettings.newBuilder();
          TextFormat.getParser().merge(br, registry, builder);
          othersSetting = builder;
        }

        /*
         * For non-tier1 carriers, DO NOT allow device overlay for now.
         * There is no easy way to generate a mononical increasing version number with overlay.
         * And if we do device overlay for a carrier, it should probobaly be tier-1.
         */

        // Bump version according to the release
        othersSetting.setVersion(
            CarrierProtoUtils.addVersionOffset(othersSetting.getVersion(), versionOffset));

        // Convert vendor specific data into binary format
        // Can be customized

        generatedMessage = othersSetting.build();
        version = othersSetting.getVersion();

      } else { // a tier-1 carrier's setting

        // Load carrier setting
        CarrierSettings.Builder carrierSetting = null;
        try (BufferedReader br = Files.newBufferedReader(baseSettingFile.toPath(), UTF_8)) {
          CarrierSettings.Builder builder = CarrierSettings.newBuilder();
          TextFormat.getParser().merge(br, registry, builder);
          carrierSetting = builder;
        }

        // Apply device overlay
        carrierSetting =
            CarrierProtoUtils.applyDeviceOverlayToCarrierSettings(deviceOverlay, carrierSetting);

        // Bump version according to the release
        carrierSetting.setVersion(
            CarrierProtoUtils.addVersionOffset(carrierSetting.getVersion(), versionOffset));

        // Convert vendor specific data into binary format
        // Can be customized

        generatedMessage = carrierSetting.build();
        version = carrierSetting.getVersion();

      }

      // Output
      String outFileMainName = childName.replace(TEXT_PB_SUFFIX, "");

      File deviceSettingTextFile = new File(deviceSettingDir, outFileMainName + TEXT_PB_SUFFIX);
      try (BufferedWriter bw = Files.newBufferedWriter(deviceSettingTextFile.toPath(), UTF_8)) {
        TextFormat.printUnicode(generatedMessage, bw);
      }

      if (!deviceInFileName.isEmpty()) {
        outFileMainName = deviceInFileName + "-" + outFileMainName;
      }
      if (versionInFileName) {
        outFileMainName += "-" + version;
      }
      File deviceSettingFile = new File(deviceSettingDir, outFileMainName + PB_SUFFIX);
      try (OutputStream os = Files.newOutputStream(deviceSettingFile.toPath())) {
        generatedMessage.writeTo(os);
      }
    }
  }

  private GenDeviceSettings() {}
}
