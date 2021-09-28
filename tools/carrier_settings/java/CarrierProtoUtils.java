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

import com.google.common.base.Preconditions;
import com.google.carrier.CarrierConfig;
import com.google.carrier.CarrierId;
import com.google.carrier.CarrierMap;
import com.google.carrier.CarrierSettings;
import com.google.carrier.MultiCarrierSettings;
import com.google.carrier.VendorConfigClient;
import com.google.carrier.VendorConfigs;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/** Utility methods */
public class CarrierProtoUtils {

  /**
   * The base of release version.
   *
   * A valid release version must be a multiple of the base.
   * A file version must be smaller than the base - otherwise the version schema is broken.
   */
  public static final long RELEASE_VERSION_BASE = 1000000000L;

  /**
   * Bump the version by an offset, to differentiate release/branch.
   *
   * The input version must be smaller than RELEASE_VERSION_BASE, and the offset must be a
   * multiple of RELEASE_VERSION_BASE.
   */
  public static long addVersionOffset(long version, long offset) {

    Preconditions.checkArgument(version < RELEASE_VERSION_BASE,
        "OMG, by design the file version should be smaller than %s, but is %s.",
        RELEASE_VERSION_BASE, version);
    Preconditions.checkArgument((offset % RELEASE_VERSION_BASE) == 0,
        "The offset %s is not a multiple of %s",
        offset, RELEASE_VERSION_BASE);

    return version + offset;
  }

  /**
   * Merge configs fields in {@code patch} into {@code base}; configs sorted by key.
   * See {@link mergeCarrierConfig(CarrierConfig, CarrierConfig.Builder)}.
   */
  public static CarrierConfig mergeCarrierConfig(CarrierConfig base, CarrierConfig patch) {
    Preconditions.checkNotNull(patch);

    return mergeCarrierConfig(base, patch.toBuilder());
  }

  /**
   * Merge configs fields in {@code patch} into {@code base}; configs sorted by key.
   *
   * <p>Sorting is desired because:
   *
   * <ul>
   *   <li>1. The order doesn't matter for the consumer so sorting will not cause behavior change;
   *   <li>2. The output proto is serilized to text for human review; undeterministic order can
   *       confuse reviewers as they cannot easily tell if it's re-ordering or actual data change.
   * </ul>
   */
  public static CarrierConfig mergeCarrierConfig(CarrierConfig base, CarrierConfig.Builder patch) {
    Preconditions.checkNotNull(base);
    Preconditions.checkNotNull(patch);

    CarrierConfig.Builder baseBuilder = base.toBuilder();

    // Traverse each config in patch
    for (int i = 0; i < patch.getConfigCount(); i++) {
      CarrierConfig.Config patchConfig = patch.getConfig(i);

      // Try to find an config in base with the same key as the config from patch
      int j = 0;
      for (j = 0; j < baseBuilder.getConfigCount(); j++) {
        if (baseBuilder.getConfig(j).getKey().equals(patchConfig.getKey())) {
          break;
        }
      }

      // If match found, replace base with patch; otherwise append patch into base.
      if (j < baseBuilder.getConfigCount()) {
        baseBuilder.setConfig(j, patchConfig);
      } else {
        baseBuilder.addConfig(patchConfig);
      }
    }

    // Sort configs in baseBuilder by key
    List<CarrierConfig.Config> configs = new ArrayList<>(baseBuilder.getConfigList());
    Collections.sort(configs, Comparator.comparing(CarrierConfig.Config::getKey));
    baseBuilder.clearConfig();
    baseBuilder.addAllConfig(configs);

    return baseBuilder.build();
  }

  /**
   * Find a carrier's CarrierSettings by canonical_name from a MultiCarrierSettings.
   *
   * <p>Return null if not found.
   */
  public static CarrierSettings findCarrierSettingsByCanonicalName(
      MultiCarrierSettings mcs, String name) {

    Preconditions.checkNotNull(mcs);
    Preconditions.checkNotNull(name);

    for (int i = 0; i < mcs.getSettingCount(); i++) {
      CarrierSettings cs = mcs.getSetting(i);
      if (cs.getCanonicalName().equals(name)) {
        return cs;
      }
    }

    return null;
  }

  /** Apply device overly to a carrier setting */
  public static CarrierSettings.Builder applyDeviceOverlayToCarrierSettings(
      MultiCarrierSettings allOverlay, CarrierSettings.Builder base) {
    return applyDeviceOverlayToCarrierSettings(allOverlay, base.build());
  }

  /** Apply device overly to a carrier setting */
  public static CarrierSettings.Builder applyDeviceOverlayToCarrierSettings(
      MultiCarrierSettings allOverlay, CarrierSettings base) {

    Preconditions.checkNotNull(allOverlay);
    Preconditions.checkNotNull(base);

    // Find overlay of the base carrier. If not found, just return base.
    CarrierSettings overlay =
        findCarrierSettingsByCanonicalName(allOverlay, base.getCanonicalName());
    if (overlay == null) {
      return base.toBuilder();
    }

    CarrierSettings.Builder resultBuilder = base.toBuilder();
    // Add version number of base settings and overlay, so the result version number
    // monotonically increases.
    resultBuilder.setVersion(base.getVersion() + overlay.getVersion());
    // Merge overlay settings into base settings
    resultBuilder.setConfigs(mergeCarrierConfig(resultBuilder.getConfigs(), overlay.getConfigs()));
    // Replace base apns with overlay apns (if not empty)
    if (overlay.getApns().getApnCount() > 0) {
      resultBuilder.setApns(overlay.getApns());
    }
    // Merge the overlay vendor configuration into base vendor configuration
    // Can be cutomized
    return resultBuilder;
  }

  /** Apply device overly to multiple carriers setting */
  public static MultiCarrierSettings applyDeviceOverlayToMultiCarrierSettings(
      MultiCarrierSettings overlay, MultiCarrierSettings base) {

    Preconditions.checkNotNull(overlay);
    Preconditions.checkNotNull(base);

    MultiCarrierSettings.Builder resultBuilder = base.toBuilder().clearSetting();
    long version = 0L;

    for (CarrierSettings cs : base.getSettingList()) {
      // Apply overlay and put overlayed carrier setting back
      CarrierSettings.Builder merged = applyDeviceOverlayToCarrierSettings(overlay, cs);
      resultBuilder.addSetting(merged);
      // The top-level version number is the sum of all version numbers of settings
      version += merged.getVersion();
    }
    resultBuilder.setVersion(version);

    return resultBuilder.build();
  }

  /**
   * Sort a list of CarrierMap with single CarrierId.
   *
   * <p>Precondition: no duplication in input list
   *
   * <p>Order by:
   *
   * <ul>
   *   <li>mcc_mnc
   *   <li>(for the same mcc_mnc) any mvno_data comes before MVNODATA_NOT_SET (Preconditon: there is
   *       only one entry with MVNODATA_NOT_SET per mcc_mnc otherwise they're duplicated)
   *   <li>(for MVNOs of the same mcc_mnc) mvno_data case + value as string
   * </ul>
   */
  public static void sortCarrierMapEntries(List<CarrierMap> list) {
    final Comparator<CarrierMap> byMccMnc =
        Comparator.comparing(
            (cm) -> {
              return cm.getCarrierId(0).getMccMnc();
            });
    final Comparator<CarrierMap> mvnoFirst =
        Comparator.comparingInt(
            (cm) -> {
              switch (cm.getCarrierId(0).getMvnoDataCase()) {
                case MVNODATA_NOT_SET:
                  return 1;
                default:
                  return 0;
              }
            });
    final Comparator<CarrierMap> byMvnoDataCaseValue =
        Comparator.comparing(
            (cm) -> {
              final CarrierId cid = cm.getCarrierId(0);
              switch (cid.getMvnoDataCase()) {
                case GID1:
                  return "GID1=" + cid.getGid1();
                case IMSI:
                  return "IMSI=" + cid.getImsi();
                case SPN:
                  return "SPN=" + cid.getSpn();
                case MVNODATA_NOT_SET:
                  throw new AssertionError("MNO should not be compared here but in `mvnoFirst`");
              }
              throw new AssertionError("uncaught case " + cid.getMvnoDataCase());
            });
    Collections.sort(list, byMccMnc.thenComparing(mvnoFirst).thenComparing(byMvnoDataCaseValue));
  }
}
