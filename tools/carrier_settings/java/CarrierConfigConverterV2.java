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

import static com.google.common.collect.Multimaps.flatteningToMultimap;
import static com.google.common.collect.Multimaps.toMultimap;
import static java.nio.charset.StandardCharsets.UTF_8;
import static java.util.Comparator.comparing;

import com.beust.jcommander.JCommander;
import com.beust.jcommander.Parameter;
import com.beust.jcommander.Parameters;
import com.google.auto.value.AutoValue;
import com.google.common.base.Ascii;
import com.google.common.base.CharMatcher;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Multimap;
import com.google.common.collect.MultimapBuilder;
import com.google.protobuf.Descriptors;
import com.google.protobuf.TextFormat;
import com.google.carrier.CarrierConfig;
import com.google.carrier.CarrierId;
import com.google.carrier.CarrierList;
import com.google.carrier.CarrierMap;
import com.google.carrier.CarrierSettings;
import com.google.carrier.IntArray;
import com.google.carrier.MultiCarrierSettings;
import com.google.carrier.TextArray;
import com.android.providers.telephony.CarrierIdProto.CarrierAttribute;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.NamedNodeMap;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;

/**
 * This command converts carrier config XML into text protobuf.
 *
 * <ul>
 *   <li>input: the assets/ from AOSP CarrierConfig app
 *   <li>input: vendor.xml file(s) which override(s) assets
 *   <li>input: a tier-1 carrier list in text protobuf (in --output_dir)
 *   <li>input: the version number for output files
 *   <li>output: an other_carriers.textpb - a list of other (non-tier-1) carriers
 *   <li>output: an others.textpb containing carrier configs for non tier-1 carriers
 *   <li>output: a .textpb for every single tier-1 carrier
 * </ul>
 */
@Parameters(separators = "=")
public final class CarrierConfigConverterV2 {
  @Parameter(names = "--assets", description = "The source AOSP assets/ directory.")
  private String assetsDirName = "/tmp/carrierconfig/assets";

  @Parameter(
      names = "--vendor_xml",
      description =
          "The source vendor.xml file(s). If multiple files provided, the order decides config"
              + " precedence, ie. configs in a file are overwritten by configs in files AFTER it.")
  private List<String> vendorXmlFiles = ImmutableList.of("/tmp/carrierconfig/vendor.xml");

  @Parameter(
      names = "--output_dir",
      description = "The destination data directory, with tier1_carriers.textpb in it.")
  private String outputDir = "/tmp/carrierconfig/out";

  @Parameter(names = "--version", description = "The version number for all output textpb.")
  private long version = 1L;

  // For configs in vendor.xml w/o mcc/mnc, they are the default config values for all carriers.
  // In CarrierSettings, a special mcc/mnc "000000" is used to look up default config.
  private static final String MCCMNC_FOR_DEFAULT_SETTINGS = "000000";

  // Resource file path to the AOSP carrier list file
  private static final String RESOURCE_CARRIER_LIST =
      "/assets/latest_carrier_id/carrier_list.textpb";

  // Constants used in parsing XMLs.
  private static final String XML_SUFFIX = ".xml";
  private static final String CARRIER_CONFIG_MCCMNC_XML_PREFIX = "carrier_config_mccmnc_";
  private static final String CARRIER_CONFIG_CID_XML_PREFIX = "carrier_config_carrierid_";
  private static final String KEY_MCCMNC_PREFIX = "mccmnc_";
  private static final String KEY_CID_PREFIX = "cid_";
  private static final String TAG_CARRIER_CONFIG = "carrier_config";

  /** Entry point when invoked from command line. */
  public static void main(String[] args) throws IOException {
    CarrierConfigConverterV2 converter = new CarrierConfigConverterV2();
    new JCommander(converter, args);
    converter.convert();
  }

  /** Entry point when invoked from other Java code, eg. the server side conversion tool. */
  public static void convert(
      String vendorXmlFile, String assetsDirName, String outputDir, long version)
      throws IOException {
    CarrierConfigConverterV2 converter = new CarrierConfigConverterV2();
    converter.vendorXmlFiles = ImmutableList.of(vendorXmlFile);
    converter.assetsDirName = assetsDirName;
    converter.outputDir = outputDir;
    converter.version = version;
    converter.convert();
  }

  private void convert() throws IOException {
    String carriersTextpbFile = getPathAsString(outputDir, "tier1_carriers.textpb");
    String settingsTextpbDir = getPathAsString(outputDir, "setting");
    CarrierList tier1Carriers;
    ArrayList<CarrierMap> otherCarriers = new ArrayList<>();
    ArrayList<String> outFiles = new ArrayList<>();
    HashMap<CarrierId, Map<String, CarrierConfig.Config>> rawConfigs = new HashMap<>();
    TreeMap<String, CarrierConfig> tier1Configs = new TreeMap<>();
    TreeMap<String, CarrierConfig> othersConfigs = new TreeMap<>();
    DocumentBuilder xmlDocBuilder = getDocumentBuilder();
    Multimap<Integer, CarrierId> aospCarrierList = loadAospCarrierList();
    Multimap<CarrierId, Integer> reverseAospCarrierList = reverseAospCarrierList(aospCarrierList);

    /*
     * High-level flow:
     * 1. Parse all input XMLs into memory
     * 2. Collect a list of interested carriers from input, represented by CarrierId.
     * 2. For each CarrierId, build its carreir configs, following AOSP DefaultCarrierConfigService.
     * 3. Merge CarrierId's as per tier1_carriers.textpb
     */

    // 1. Parse all input XMLs into memory
    Map<String, Document> assetsXmls = new HashMap<>();
    List<Document> vendorXmls = new ArrayList<>();
    // Parse assets/carrier_config_*.xml
    for (File childFile : new File(assetsDirName).listFiles()) {
      String childFileName = childFile.getName();
      String fullChildName = childFile.getCanonicalPath();
      if (childFileName.startsWith(CARRIER_CONFIG_MCCMNC_XML_PREFIX)) {
        String mccMnc =
            childFileName.substring(
                CARRIER_CONFIG_MCCMNC_XML_PREFIX.length(), childFileName.indexOf(XML_SUFFIX));
        if (!mccMnc.matches("\\d{5,6}")) {
          throw new IOException("Invalid mcc/mnc " + mccMnc + " found in " + childFileName);
        }
        try {
          assetsXmls.put(KEY_MCCMNC_PREFIX + mccMnc, parseXmlDoc(fullChildName, xmlDocBuilder));
        } catch (SAXException | IOException e) {
          throw new IOException("Failed to parse " + childFileName, e);
        }
      } else if (childFileName.startsWith(CARRIER_CONFIG_CID_XML_PREFIX)) {
        String cidAndCarrierName =
            childFileName.substring(
                CARRIER_CONFIG_CID_XML_PREFIX.length(), childFileName.indexOf(XML_SUFFIX));
        int cid = -1;
        try {
          cid = Integer.parseInt(cidAndCarrierName.split("_", -1)[0]);
        } catch (NumberFormatException e) {
          throw new IOException("Invalid carrierid found in " + childFileName, e);
        }
        try {
          assetsXmls.put(KEY_CID_PREFIX + cid, parseXmlDoc(fullChildName, xmlDocBuilder));
        } catch (SAXException | IOException e) {
          throw new IOException("Failed to parse " + childFileName, e);
        }
      }
      // ignore other malformatted files.
    }
    // Parse vendor.xml files
    for (String vendorXmlFile : vendorXmlFiles) {
      try {
        vendorXmls.add(parseXmlDoc(vendorXmlFile, xmlDocBuilder));
      } catch (SAXException | IOException e) {
        throw new IOException("Failed to parse " + vendorXmlFile, e);
      }
    }

    // 2. Collect all carriers from input, represented by CarrierId.
    List<CarrierId> carriers = new ArrayList<>();
    // Traverse <carrier_config /> labels in each file.
    for (Map.Entry<String, Document> xml : assetsXmls.entrySet()) {
      if (xml.getKey().startsWith(KEY_MCCMNC_PREFIX)) {
        String mccMnc = xml.getKey().substring(KEY_MCCMNC_PREFIX.length());
        for (Element element : getElementsByTagName(xml.getValue(), TAG_CARRIER_CONFIG)) {
          try {
            CarrierId id = parseCarrierId(element).setMccMnc(mccMnc).build();
            carriers.add(id);
          } catch (UnsupportedOperationException e) {
            throw new IOException("Unsupported syntax in assets/ for " + mccMnc, e);
          }
        }
      } else if (xml.getKey().startsWith(KEY_CID_PREFIX)) {
        int cid = Integer.parseInt(xml.getKey().substring(KEY_CID_PREFIX.length()));
        if (aospCarrierList.containsKey(cid)) {
          carriers.addAll(aospCarrierList.get(cid));
        } else {
          System.err.printf("Undefined cid %d in assets/. Ignore.\n", cid);
        }
      }
    }
    for (Document vendorXml : vendorXmls) {
      for (Element element : getElementsByTagName(vendorXml, TAG_CARRIER_CONFIG)) {
        // First, try to parse cid
        if (element.hasAttribute("cid")) {
          String cidAsString = element.getAttribute("cid");
          int cid = Integer.parseInt(cidAsString);
          if (aospCarrierList.containsKey(cid)) {
            carriers.addAll(aospCarrierList.get(cid));
          } else {
            System.err.printf("Undefined cid %d in vendor.xml. Ignore.\n", cid);
          }
        } else {
          // Then, try to parse CarrierId
          CarrierId.Builder id = parseCarrierId(element);
          // A valid mccmnc is 5- or 6-digit. But vendor.xml see special cases below:
          // Case 1: a <carrier_config> element may have neither "mcc" nor "mnc".
          // Such a tag provides a base config value for all carriers. CarrierSettings uses
          // mcc/mnc 000/000 to identify that, and does the merge at run time instead of
          // build time (which is now).
          // Case 2: a <carrier_config> element may have just "mcc" and not "mnc" for
          // country-wise config. Such a element doesn't make a carrier; but still keep it so
          // can be used if a mccmnc appears in APNs later.
          if (id.getMccMnc().isEmpty()) {
            // special case 1
            carriers.add(id.setMccMnc(MCCMNC_FOR_DEFAULT_SETTINGS).build());
          } else if (id.getMccMnc().length() == 3) {
            // special case 2
            carriers.add(id.build());
          } else if (id.getMccMnc().length() == 5 || id.getMccMnc().length() == 6) {
            // Normal mcc+mnc
            carriers.add(id.build());
          } else {
            System.err.printf("Invalid mcc/mnc: %s. Ignore.\n", id.getMccMnc());
          }
        }
      }
    }

    // 3. For each CarrierId, build its carrier configs, following AOSP DefaultCarrierConfigService.
    for (CarrierId carrier : carriers) {
      Map<String, CarrierConfig.Config> config = ImmutableMap.of();

      CarrierIdentifier id = getCid(carrier, reverseAospCarrierList);
      if (id.getCarrierId() != -1) {
        HashMap<String, CarrierConfig.Config> configBySpecificCarrierId =
            parseCarrierConfigFromXml(
                assetsXmls.get(KEY_CID_PREFIX + id.getSpecificCarrierId()), id);
        HashMap<String, CarrierConfig.Config> configByCarrierId =
            parseCarrierConfigFromXml(assetsXmls.get(KEY_CID_PREFIX + id.getCarrierId()), id);
        HashMap<String, CarrierConfig.Config> configByMccMncFallBackCarrierId =
            parseCarrierConfigFromXml(assetsXmls.get(KEY_CID_PREFIX + id.getMccmncCarrierId()), id);
        // priority: specific carrier id > carrier id > mccmnc fallback carrier id
        if (!configBySpecificCarrierId.isEmpty()) {
          config = configBySpecificCarrierId;
        } else if (!configByCarrierId.isEmpty()) {
          config = configByCarrierId;
        } else if (!configByMccMncFallBackCarrierId.isEmpty()) {
          config = configByMccMncFallBackCarrierId;
        }
      }
      if (config.isEmpty()) {
        // fallback to use mccmnc.xml when there is no carrier id named configuration found.
        config =
            parseCarrierConfigFromXml(assetsXmls.get(KEY_MCCMNC_PREFIX + carrier.getMccMnc()), id);
      }
      // Treat vendor.xml files as if they were appended to the carrier configs read from assets.
      for (Document vendorXml : vendorXmls) {
        HashMap<String, CarrierConfig.Config> vendorConfig =
            parseCarrierConfigFromVendorXml(vendorXml, id);
        config.putAll(vendorConfig);
      }

      rawConfigs.put(carrier, config);
    }

    // Read tier1_carriers.textpb
    try (InputStream carriersTextpb = new FileInputStream(new File(carriersTextpbFile));
        BufferedReader br = new BufferedReader(new InputStreamReader(carriersTextpb, UTF_8))) {
      CarrierList.Builder builder = CarrierList.newBuilder();
      TextFormat.getParser().merge(br, builder);
      tier1Carriers = builder.build();
    }

    // Compose tier1Configs and othersConfigs from rawConfigs
    rawConfigs.forEach(
        (carrierId, configs) -> {
          String cname = getCanonicalName(tier1Carriers, carrierId);
          CarrierConfig.Builder ccb = toCarrierConfigBuilder(configs);
          if (cname != null) { // tier-1 carrier
            if (tier1Configs.containsKey(cname)) {
              tier1Configs.put(
                  cname, CarrierProtoUtils.mergeCarrierConfig(tier1Configs.get(cname), ccb));
            } else {
              tier1Configs.put(cname, ccb.build());
            }
          } else { // other carrier
            cname = generateCanonicalNameForOthers(carrierId);
            otherCarriers.add(
                CarrierMap.newBuilder().addCarrierId(carrierId).setCanonicalName(cname).build());
            othersConfigs.put(cname, ccb.build());
          }
        });

    // output tier1 carrier settings
    for (int i = 0; i < tier1Carriers.getEntryCount(); i++) {
      CarrierMap cm = tier1Carriers.getEntry(i);
      String cname = cm.getCanonicalName();
      String fileName = getPathAsString(settingsTextpbDir, cname + ".textpb");

      outFiles.add(fileName);

      try (OutputStream os = new FileOutputStream(new File(fileName));
          BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(os, UTF_8))) {
        CarrierSettings.Builder cs = CarrierSettings.newBuilder().setCanonicalName(cname);
        if (tier1Configs.containsKey(cname)) {
          cs.setConfigs(sortConfig(tier1Configs.get(cname)).toBuilder().build());
        }
        cs.setVersion(version);
        TextFormat.printUnicode(cs.build(), bw);
      }
    }

    // Output other carriers list
    String otherCarriersFile = getPathAsString(outputDir, "other_carriers.textpb");
    outFiles.add(otherCarriersFile);
    CarrierProtoUtils.sortCarrierMapEntries(otherCarriers);
    try (OutputStream os = new FileOutputStream(new File(otherCarriersFile));
        BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(os, UTF_8))) {
      CarrierList cl =
          CarrierList.newBuilder().addAllEntry(otherCarriers).setVersion(version).build();
      TextFormat.printUnicode(cl, bw);
    }

    // Output other carriers settings
    String othersFileName = getPathAsString(settingsTextpbDir, "others.textpb");
    outFiles.add(othersFileName);
    try (OutputStream os = new FileOutputStream(new File(othersFileName));
        BufferedWriter bw = new BufferedWriter(new OutputStreamWriter(os, UTF_8))) {
      MultiCarrierSettings.Builder mcs = MultiCarrierSettings.newBuilder().setVersion(version);
      othersConfigs.forEach(
          (cname, cc) -> {
            mcs.addSetting(
                CarrierSettings.newBuilder()
                    .setCanonicalName(cname)
                    .setConfigs(sortConfig(cc).toBuilder().build())
                    .build());
          });
      TextFormat.printUnicode(mcs.build(), bw);
    }

    // Print out the list of all output file names
    System.out.println("SUCCESS! Files generated:");
    for (String fileName : outFiles) {
      System.out.println(fileName);
    }
  }

  private static DocumentBuilder getDocumentBuilder() {
    try {
      DocumentBuilderFactory dbFactory = DocumentBuilderFactory.newInstance();
      return dbFactory.newDocumentBuilder();
    } catch (ParserConfigurationException e) {
      throw new IllegalStateException(e);
    }
  }

  private static Multimap<Integer, CarrierId> loadAospCarrierList() throws IOException {
    com.android.providers.telephony.CarrierIdProto.CarrierList.Builder aospCarrierList =
        com.android.providers.telephony.CarrierIdProto.CarrierList.newBuilder();
    try (InputStream textpb =
            CarrierConfigConverterV2.class.getResourceAsStream(RESOURCE_CARRIER_LIST);
        BufferedReader textpbReader = new BufferedReader(new InputStreamReader(textpb, UTF_8))) {
      TextFormat.getParser().merge(textpbReader, aospCarrierList);
    }
    return aospCarrierList.getCarrierIdList().stream()
        .collect(
            flatteningToMultimap(
                cid -> cid.getCanonicalId(),
                cid -> carrierAttributeToCarrierId(cid.getCarrierAttributeList()).stream(),
                MultimapBuilder.linkedHashKeys().arrayListValues()::build));
  }

  // Convert `CarrierAttribute`s to `CarrierId`s.
  // A CarrierAttribute message with fields not supported by CarrierSettings, like preferred_apn,
  // is ignored.
  private static ImmutableList<CarrierId> carrierAttributeToCarrierId(
      List<CarrierAttribute> carrierAttributes) {
    List<CarrierId> result = new ArrayList<>();
    ImmutableSet<Descriptors.FieldDescriptor> supportedFields =
        ImmutableSet.of(
            CarrierAttribute.getDescriptor().findFieldByName("mccmnc_tuple"),
            CarrierAttribute.getDescriptor().findFieldByName("imsi_prefix_xpattern"),
            CarrierAttribute.getDescriptor().findFieldByName("spn"),
            CarrierAttribute.getDescriptor().findFieldByName("gid1"));
    for (CarrierAttribute carrierAttribute : carrierAttributes) {
      if (!carrierAttribute.getAllFields().keySet().stream().allMatch(supportedFields::contains)) {
        // This `CarrierAttribute` contains unsupported fields; skip.
        continue;
      }
      for (String mccmnc : carrierAttribute.getMccmncTupleList()) {
        CarrierId.Builder carrierId = CarrierId.newBuilder().setMccMnc(mccmnc);
        if (carrierAttribute.getImsiPrefixXpatternCount() > 0) {
          for (String imsi : carrierAttribute.getImsiPrefixXpatternList()) {
            result.add(carrierId.setImsi(imsi).build());
          }
        } else if (carrierAttribute.getGid1Count() > 0) {
          for (String gid1 : carrierAttribute.getGid1List()) {
            result.add(carrierId.setGid1(gid1).build());
          }
        } else if (carrierAttribute.getSpnCount() > 0) {
          for (String spn : carrierAttribute.getSpnList()) {
            // Some SPN has trailng space character \r, messing up textpb. Remove them.
            // It won't affect CarrierSettings which uses prefix matching for SPN.
            result.add(carrierId.setSpn(CharMatcher.whitespace().trimTrailingFrom(spn)).build());
          }
        } else { // Ignore other attributes not supported by CarrierSettings
          result.add(carrierId.build());
        }
      }
    }
    // Dedup
    return ImmutableSet.copyOf(result).asList();
  }

  private static Multimap<CarrierId, Integer> reverseAospCarrierList(
      Multimap<Integer, CarrierId> aospCarrierList) {
    return aospCarrierList.entries().stream()
        .collect(
            toMultimap(
                entry -> entry.getValue(),
                entry -> entry.getKey(),
                MultimapBuilder.linkedHashKeys().arrayListValues()::build));
  }

  private static Document parseXmlDoc(String fileName, DocumentBuilder xmlDocBuilder)
      throws SAXException, IOException {
    try (InputStream configXml = new FileInputStream(new File(fileName))) {
      Document xmlDoc = xmlDocBuilder.parse(configXml);
      xmlDoc.getDocumentElement().normalize();
      return xmlDoc;
    }
  }

  private static ImmutableList<Element> getElementsByTagName(Document xmlDoc, String tagName) {
    if (xmlDoc == null) {
      return ImmutableList.of();
    }
    ImmutableList.Builder<Element> result = new ImmutableList.Builder<>();
    xmlDoc.getDocumentElement().normalize();
    NodeList nodeList = xmlDoc.getElementsByTagName(tagName);
    for (int i = 0; i < nodeList.getLength(); i++) {
      Node node = nodeList.item(i);
      if (node.getNodeType() == Node.ELEMENT_NODE) {
        result.add((Element) node);
      }
    }
    return result.build();
  }

  static CarrierConfig sortConfig(CarrierConfig in) {
    final CarrierConfig.Builder result = in.toBuilder().clearConfig();
    in.getConfigList().stream()
        .sorted(comparing(CarrierConfig.Config::getKey))
        .forEachOrdered((c) -> result.addConfig(c));
    return result.build();
  }

  static String getCanonicalName(CarrierList pList, CarrierId pId) {
    for (int i = 0; i < pList.getEntryCount(); i++) {
      CarrierMap cm = pList.getEntry(i);
      for (int j = 0; j < cm.getCarrierIdCount(); j++) {
        CarrierId cid = cm.getCarrierId(j);
        if (cid.equals(pId)) {
          return cm.getCanonicalName();
        }
      }
    }
    return null;
  }

  static String generateCanonicalNameForOthers(CarrierId pId) {
    // Not a tier-1 carrier: generate name
    StringBuilder genName = new StringBuilder(pId.getMccMnc());
    switch (pId.getMvnoDataCase()) {
      case GID1:
        genName.append("GID1=");
        genName.append(Ascii.toUpperCase(pId.getGid1()));
        break;
      case SPN:
        genName.append("SPN=");
        genName.append(Ascii.toUpperCase(pId.getSpn()));
        break;
      case IMSI:
        genName.append("IMSI=");
        genName.append(Ascii.toUpperCase(pId.getImsi()));
        break;
      default: // MVNODATA_NOT_SET
        // Do nothing
    }
    return genName.toString();
  }

  /**
   * Converts a map with carrier configs to a {@link CarrierConfig.Builder}.
   *
   * @see #parseCarrierConfigToMap
   */
  private static CarrierConfig.Builder toCarrierConfigBuilder(
      Map<String, CarrierConfig.Config> configs) {
    CarrierConfig.Builder builder = CarrierConfig.newBuilder();
    configs.forEach(
        (key, value) -> {
          builder.addConfig(value.toBuilder().setKey(key));
        });
    return builder;
  }

  /**
   * Returns a map with carrier configs parsed from a assets/*.xml.
   *
   * @return a map, key being the carrier config key, value being a {@link CarrierConfig.Config}
   *     with one of the value set.
   */
  private static HashMap<String, CarrierConfig.Config> parseCarrierConfigFromXml(
      Document xmlDoc, CarrierIdentifier carrier) throws IOException {
    HashMap<String, CarrierConfig.Config> configMap = new HashMap<>();
    for (Element element : getElementsByTagName(xmlDoc, TAG_CARRIER_CONFIG)) {
      if (carrier != null && !checkFilters(false, element, carrier)) {
        continue;
      }
      configMap.putAll(parseCarrierConfigToMap(element));
    }
    return configMap;
  }

  /**
   * Returns a map with carrier configs parsed from the vendor.xml.
   *
   * @return a map, key being the carrier config key, value being a {@link CarrierConfig.Config}
   *     with one of the value set.
   */
  private static HashMap<String, CarrierConfig.Config> parseCarrierConfigFromVendorXml(
      Document xmlDoc, CarrierIdentifier carrier) throws IOException {
    HashMap<String, CarrierConfig.Config> configMap = new HashMap<>();
    for (Element element : getElementsByTagName(xmlDoc, TAG_CARRIER_CONFIG)) {
      if (carrier != null && !checkFilters(true, element, carrier)) {
        continue;
      }
      configMap.putAll(parseCarrierConfigToMap(element));
    }
    return configMap;
  }

  /**
   * Returns a map with carrier configs parsed from the XML element.
   *
   * @return a map, key being the carrier config key, value being a {@link CarrierConfig.Config}
   *     with one of the value set.
   */
  private static HashMap<String, CarrierConfig.Config> parseCarrierConfigToMap(Element element)
      throws IOException {
    HashMap<String, CarrierConfig.Config> configMap = new HashMap<>();
    NodeList nList;
    // bool value
    nList = element.getElementsByTagName("boolean");
    for (int i = 0; i < nList.getLength(); i++) {
      Node nNode = nList.item(i);
      if (nNode.getNodeType() != Node.ELEMENT_NODE) {
        continue;
      }
      Element eElement = (Element) nNode;
      String key = eElement.getAttribute("name");
      boolean value = Boolean.parseBoolean(eElement.getAttribute("value"));
      configMap.put(key, CarrierConfig.Config.newBuilder().setBoolValue(value).build());
    }
    // int value
    nList = element.getElementsByTagName("int");
    for (int i = 0; i < nList.getLength(); i++) {
      Node nNode = nList.item(i);
      if (nNode.getNodeType() != Node.ELEMENT_NODE) {
        continue;
      }
      Element eElement = (Element) nNode;
      String key = eElement.getAttribute("name");
      int value = Integer.parseInt(eElement.getAttribute("value"));
      configMap.put(key, CarrierConfig.Config.newBuilder().setIntValue(value).build());
    }
    // long value
    nList = element.getElementsByTagName("long");
    for (int i = 0; i < nList.getLength(); i++) {
      Node nNode = nList.item(i);
      if (nNode.getNodeType() != Node.ELEMENT_NODE) {
        continue;
      }
      Element eElement = (Element) nNode;
      String key = eElement.getAttribute("name");
      long value = Long.parseLong(eElement.getAttribute("value"));
      configMap.put(key, CarrierConfig.Config.newBuilder().setLongValue(value).build());
    }
    // text value
    nList = element.getElementsByTagName("string");
    for (int i = 0; i < nList.getLength(); i++) {
      Node nNode = nList.item(i);
      if (nNode.getNodeType() != Node.ELEMENT_NODE) {
        continue;
      }
      Element eElement = (Element) nNode;
      String key = eElement.getAttribute("name");
      String value = String.valueOf(eElement.getTextContent());
      configMap.put(key, CarrierConfig.Config.newBuilder().setTextValue(value).build());
    }
    // text array
    nList = element.getElementsByTagName("string-array");
    for (int i = 0; i < nList.getLength(); i++) {
      Node nNode = nList.item(i);
      if (nNode.getNodeType() != Node.ELEMENT_NODE) {
        continue;
      }
      Element eElement = (Element) nNode;
      String key = eElement.getAttribute("name");
      CarrierConfig.Config.Builder cccb = CarrierConfig.Config.newBuilder();
      TextArray.Builder cctb = TextArray.newBuilder();
      NodeList subList = eElement.getElementsByTagName("item");
      for (int j = 0; j < subList.getLength(); j++) {
        Node subNode = subList.item(j);
        if (subNode.getNodeType() != Node.ELEMENT_NODE) {
          continue;
        }
        Element subElement = (Element) subNode;
        String value = String.valueOf(subElement.getAttribute("value"));
        cctb.addItem(value);
      }
      configMap.put(key, cccb.setTextArray(cctb.build()).build());
    }
    // bool array
    nList = element.getElementsByTagName("int-array");
    for (int i = 0; i < nList.getLength(); i++) {
      Node nNode = nList.item(i);
      if (nNode.getNodeType() != Node.ELEMENT_NODE) {
        continue;
      }
      Element eElement = (Element) nNode;
      String key = eElement.getAttribute("name");
      CarrierConfig.Config.Builder cccb = CarrierConfig.Config.newBuilder();
      IntArray.Builder ccib = IntArray.newBuilder();
      NodeList subList = eElement.getElementsByTagName("item");
      for (int j = 0; j < subList.getLength(); j++) {
        Node subNode = subList.item(j);
        if (subNode.getNodeType() != Node.ELEMENT_NODE) {
          continue;
        }
        Element subElement = (Element) subNode;
        int value = Integer.parseInt(subElement.getAttribute("value"));
        ccib.addItem(value);
      }
      configMap.put(key, cccb.setIntArray(ccib.build()).build());
    }
    return configMap;
  }

  /**
   * Returns {@code true} if a <carrier_config ...> element matches the carrier identifier.
   *
   * <p>Copied from AOSP DefaultCarrierConfigService.
   */
  private static boolean checkFilters(boolean isVendorXml, Element element, CarrierIdentifier id) {
    // Special case: in vendor.xml, the <carrier_config> element w/o mcc/mnc provides a base config
    // value for all carriers. CarrierSettings uses mcc/mnc 000/000 to identify that, and does the
    // merge at run time instead of build time (which is now).
    // Hence, such an element should only match 000/000.
    if (isVendorXml
        && !element.hasAttribute("mcc")
        && !element.hasAttribute("mnc")
        && !element.hasAttribute("cid")) {
      return MCCMNC_FOR_DEFAULT_SETTINGS.equals(id.getMcc() + id.getMnc());
    }
    boolean result = true;
    NamedNodeMap attributes = element.getAttributes();
    for (int i = 0; i < attributes.getLength(); i++) {
      String attribute = attributes.item(i).getNodeName();
      String value = attributes.item(i).getNodeValue();
      switch (attribute) {
        case "mcc":
          result = result && value.equals(id.getMcc());
          break;
        case "mnc":
          result = result && value.equals(id.getMnc());
          break;
        case "gid1":
          result = result && Ascii.equalsIgnoreCase(value, id.getGid1());
          break;
        case "spn":
          result = result && matchOnSP(value, id);
          break;
        case "imsi":
          result = result && matchOnImsi(value, id);
          break;
        case "cid":
          result =
              result
                  && ((Integer.parseInt(value) == id.getCarrierId())
                      || (Integer.parseInt(value) == id.getSpecificCarrierId()));
          break;
        case "name":
          // name is used together with cid for readability. ignore for filter.
          break;
        default:
          System.err.println("Unsupported attribute " + attribute + "=" + value);
          result = false;
      }
    }
    return result;
  }

  /**
   * Returns {@code true} if an "spn" attribute in <carrier_config ...> element matches the carrier
   * identifier.
   *
   * <p>Copied from AOSP DefaultCarrierConfigService.
   */
  private static boolean matchOnSP(String xmlSP, CarrierIdentifier id) {
    boolean matchFound = false;

    String currentSP = id.getSpn();
    // <carrier_config ... spn="null"> means expecting SIM SPN empty in AOSP convention.
    if (Ascii.equalsIgnoreCase("null", xmlSP)) {
      if (currentSP.isEmpty()) {
        matchFound = true;
      }
    } else if (currentSP != null) {
      Pattern spPattern = Pattern.compile(xmlSP, Pattern.CASE_INSENSITIVE);
      Matcher matcher = spPattern.matcher(currentSP);
      matchFound = matcher.matches();
    }
    return matchFound;
  }

  /**
   * Returns {@code true} if an "imsi" attribute in <carrier_config ...> element matches the carrier
   * identifier.
   *
   * <p>Copied from AOSP DefaultCarrierConfigService.
   */
  private static boolean matchOnImsi(String xmlImsi, CarrierIdentifier id) {
    boolean matchFound = false;

    String currentImsi = id.getImsi();
    // If we were able to retrieve current IMSI, see if it matches.
    if (currentImsi != null) {
      Pattern imsiPattern = Pattern.compile(xmlImsi, Pattern.CASE_INSENSITIVE);
      Matcher matcher = imsiPattern.matcher(currentImsi);
      matchFound = matcher.matches();
    }
    return matchFound;
  }

  /**
   * Parses a {@link CarrierId} out of a <carrier_config ...> tag.
   *
   * <p>This is purely used for discover potential carriers expressed by this tag, the return value
   * may not reflect all attributes of the tag.
   */
  private static CarrierId.Builder parseCarrierId(Element element) {
    CarrierId.Builder builder = CarrierId.newBuilder();
    String mccMnc = element.getAttribute("mcc") + element.getAttribute("mnc");
    builder.setMccMnc(mccMnc);
    if (element.hasAttribute("imsi")) {
      builder.setImsi(element.getAttribute("imsi"));
    } else if (element.hasAttribute("gid1")) {
      builder.setGid1(element.getAttribute("gid1"));
    } else if (element.hasAttribute("gid2")) {
      throw new UnsupportedOperationException(
          "Not support attribute `gid2`: " + element.getAttribute("gid2"));
    } else if (element.hasAttribute("spn")) {
      builder.setSpn(element.getAttribute("spn"));
    }
    return builder;
  }

  // Same as {@link java.nio.file.Paths#get} but returns a String
  private static String getPathAsString(String first, String... more) {
    return java.nio.file.Paths.get(first, more).toString();
  }

  /** Mirror of Android CarrierIdentifier class. Default value of a carrier id is -1. */
  @AutoValue
  abstract static class CarrierIdentifier {
    abstract String getMcc();

    abstract String getMnc();

    abstract String getImsi();

    abstract String getGid1();

    abstract String getSpn();

    abstract int getCarrierId();

    abstract int getSpecificCarrierId();

    abstract int getMccmncCarrierId();

    static CarrierIdentifier create(
        CarrierId carrier, int carrierId, int specificCarrierId, int mccmncCarrierId) {
      String mcc = carrier.getMccMnc().substring(0, 3);
      String mnc = carrier.getMccMnc().length() > 3 ? carrier.getMccMnc().substring(3) : "";
      return new AutoValue_CarrierConfigConverterV2_CarrierIdentifier(
          mcc,
          mnc,
          carrier.getImsi(),
          carrier.getGid1(),
          carrier.getSpn(),
          carrierId,
          specificCarrierId,
          mccmncCarrierId);
    }
  }

  private static CarrierIdentifier getCid(
      CarrierId carrierId, Multimap<CarrierId, Integer> reverseAospCarrierList) {
    // Mimic TelephonyManager#getCarrierIdFromMccMnc, which is implemented by
    // CarrierResolver#getCarrierIdFromMccMnc.
    CarrierId mccMnc = CarrierId.newBuilder().setMccMnc(carrierId.getMccMnc()).build();
    int mccMncCarrierId = reverseAospCarrierList.get(mccMnc).stream().findFirst().orElse(-1);

    List<Integer> cids = ImmutableList.copyOf(reverseAospCarrierList.get(carrierId));
    // No match: use -1
    if (cids.isEmpty()) {
      return CarrierIdentifier.create(carrierId, -1, -1, mccMncCarrierId);
    }
    // One match: use as both carrierId and specificCarrierId
    if (cids.size() == 1) {
      return CarrierIdentifier.create(carrierId, cids.get(0), cids.get(0), mccMncCarrierId);
    }
    // Two matches:  specificCarrierId is always bigger than carrierId
    if (cids.size() == 2) {
      return CarrierIdentifier.create(
          carrierId,
          Math.min(cids.get(0), cids.get(1)),
          Math.max(cids.get(0), cids.get(1)),
          mccMncCarrierId);
    }
    // Cannot be more than 2 matches.
    throw new IllegalStateException("More than two cid's found for " + carrierId + ": " + cids);
  }

  private CarrierConfigConverterV2() {}
}
