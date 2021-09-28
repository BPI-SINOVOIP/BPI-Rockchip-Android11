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
import com.google.carrier.CarrierId;
import com.google.carrier.CarrierList;
import com.google.carrier.CarrierMap;
import com.google.common.base.Verify;
import com.google.protobuf.TextFormat;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * This command line tool generate a single carrier_list.pb. The output pb is used as a reverse-map:
 * each entry is a CarrierMap with a canonical_name and only one CarrierId; entries are sorted by
 * mcc_mnc, and MVNO comes before MNO. So that when matching a CarrierId against this list, the
 * first match is always the best guess of carrier name.
 */
@Parameters(separators = "=")
public final class GenCarrierList {
  @Parameter(
      names = "--version_offset",
      description =
          "The value to be added to file version, used to differentiate releases/branches.")
  private long versionOffset = 0L;

  @Parameter(
      names = "--in_textpbs",
      description = "A comma-separated list of input CarrierList textpb files")
  private List<String> textpbFileNames = Arrays.asList();

  @Parameter(names = "--out_pb", description = "The output CarrierList pb file")
  private String outFileName = "/tmp/carrier_list.pb";

  @Parameter(
      names = "--with_version_number",
      description = "Encode version number into output pb filename.")
  private boolean versionInFileName = false;

  public static final String BINARY_PB_SUFFIX = ".pb";
  public static final String TEXT_PB_SUFFIX = ".textpb";

  public static void main(String[] args) throws IOException {
    GenCarrierList generator = new GenCarrierList();
    new JCommander(generator, args);
    generator.generate();
  }

  private void generate() throws IOException {
    long version = 0;

    Verify.verify(
        outFileName.endsWith(BINARY_PB_SUFFIX),
        "Output filename must end with %s.",
        BINARY_PB_SUFFIX);

    List<CarrierMap> list = new ArrayList<>();
    for (String textpbFileName : textpbFileNames) {
      // Load textpbFileName
      CarrierList carrierList = null;
      try (BufferedReader br = Files.newBufferedReader(Paths.get(textpbFileName), UTF_8)) {
        CarrierList.Builder builder = CarrierList.newBuilder();
        TextFormat.getParser().merge(br, builder);
        carrierList = builder.build();
      }

      // Add carrierList into list
      for (CarrierMap cm : carrierList.getEntryList()) {
        for (CarrierId cid : cm.getCarrierIdList()) {
          list.add(cm.toBuilder().clearCarrierId().addCarrierId(cid).build());
        }
      }

      // Add up version number
      version += carrierList.getVersion();
    }

    // Bump version according to the release
    version = CarrierProtoUtils.addVersionOffset(version, versionOffset);

    // Sort the list as per carrier identification algorithm requirement
    CarrierProtoUtils.sortCarrierMapEntries(list);

    // Convert list to CarrierList
    CarrierList clist = CarrierList.newBuilder().setVersion(version).addAllEntry(list).build();

    // Output
    String outFileMainName = outFileName.replace(BINARY_PB_SUFFIX, "");
    try (BufferedWriter bw =
        Files.newBufferedWriter(Paths.get(outFileMainName + TEXT_PB_SUFFIX), UTF_8)) {
      TextFormat.printUnicode(clist, bw);
    }

    if (versionInFileName) {
      outFileMainName += "-" + clist.getVersion();
    }
    try (OutputStream os = Files.newOutputStream(Paths.get(outFileMainName + BINARY_PB_SUFFIX))) {
      clist.writeTo(os);
    }
  }

  private GenCarrierList() {}
}
