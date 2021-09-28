/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.provider.cts;

import static android.content.pm.PackageManager.PERMISSION_GRANTED;

import static com.google.common.truth.Truth.assertThat;
import static org.junit.Assume.assumeFalse;

import android.app.slice.Slice;
import android.app.slice.SliceManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Process;

import android.net.wifi.WifiManager;
import android.util.Log;

import androidx.slice.SliceConvert;
import androidx.slice.SliceMetadata;
import androidx.slice.core.SliceAction;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import java.util.Collections;
import java.util.List;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class WifiSliceTest {
  private static final String TAG = "WifiSliceTest";

  private static final Uri WIFI_SLICE_URI =
          Uri.parse("content://android.settings.slices/action/wifi");

  private static final String ACTION_ASSIST = "android.intent.action.ASSIST";
  private static final String ACTION_VOICE_ASSIST = "android.intent.action.VOICE_ASSIST";
  private static final String CATEGORY_DEFAULT = "android.intent.category.DEFAULT";
  private static final String FEATURE_VOICE_RECOGNIZERS = "android.software.voice_recognizers";

  private final Context mContext = InstrumentationRegistry.getContext();
  private final SliceManager mSliceManager = mContext.getSystemService(SliceManager.class);
  private final boolean mHasVoiceRecognizersFeature =
          mContext.getPackageManager().hasSystemFeature(FEATURE_VOICE_RECOGNIZERS);

  private Slice mWifiSlice;

  @Before
  public void setUp() throws Exception {
    assumeFalse("Skipping test: Auto does not support provider android.settings.slices", isCar());
    assumeFalse("Skipping test: TV does not support provider android.settings.slices", isTv());
    mWifiSlice = mSliceManager.bindSlice(WIFI_SLICE_URI, Collections.emptySet());
  }

  @Test
  public void wifiSliceToggle_changeWifiState() {
    SliceMetadata mWifiSliceMetadata =
            SliceMetadata.from(mContext, SliceConvert.wrap(mWifiSlice, mContext));
    List<SliceAction> wifiSliceActions = mWifiSliceMetadata.getToggles();
    if (wifiSliceActions.size() != 0) {
      SliceAction toggleAction = wifiSliceActions.get(0);

      toggleAction.setChecked(true);
      assertThat(toggleAction.isChecked()).isEqualTo(isWifiEnabled());

      toggleAction.setChecked(false);
      assertThat(toggleAction.isChecked()).isEqualTo(isWifiEnabled());
    }
  }

  @Test
  public void wifiSlice_hasCorrectUri() {
    assertThat(mWifiSlice.getUri()).isEqualTo(WIFI_SLICE_URI);
  }

  @Test
  public void wifiSlice_grantedPermissionToDefaultAssistant() throws NameNotFoundException {
    if (!mHasVoiceRecognizersFeature) {
      Log.i(TAG, "The device doesn't support feature: " + FEATURE_VOICE_RECOGNIZERS);
      return;
    }
    final PackageManager pm = mContext.getPackageManager();
    final Intent requestDefaultAssistant =
            new Intent(ACTION_ASSIST).addCategory(CATEGORY_DEFAULT);

    final ResolveInfo info = pm.resolveActivity(requestDefaultAssistant, 0);

    if (info != null) {
      final int testPid = Process.myPid();
      final int testUid = pm.getPackageUid(info.activityInfo.packageName,  0);

      assertThat(mSliceManager.checkSlicePermission(WIFI_SLICE_URI, testPid, testUid))
              .isEqualTo(PERMISSION_GRANTED);
    }
  }

  @Test
  public void wifiSlice_grantedPermissionToDefaultVoiceAssistant() throws NameNotFoundException {
    if (!mHasVoiceRecognizersFeature) {
      Log.i(TAG, "The device doesn't support feature: " + FEATURE_VOICE_RECOGNIZERS);
      return;
    }
    final PackageManager pm = mContext.getPackageManager();
    final Intent requestDefaultAssistant =
            new Intent(ACTION_VOICE_ASSIST).addCategory(CATEGORY_DEFAULT);

    final ResolveInfo info = pm.resolveActivity(requestDefaultAssistant, 0);

    if (info != null) {
      final int testPid = Process.myPid();
      final int testUid = pm.getPackageUid(info.activityInfo.packageName,  0);

      assertThat(mSliceManager.checkSlicePermission(WIFI_SLICE_URI, testPid, testUid))
              .isEqualTo(PERMISSION_GRANTED);
    }
  }

  private boolean isCar() {
    PackageManager pm = mContext.getPackageManager();
    return pm.hasSystemFeature(PackageManager.FEATURE_AUTOMOTIVE);
  }

  private boolean isTv() {
    PackageManager pm = mContext.getPackageManager();
    return pm.hasSystemFeature(PackageManager.FEATURE_TELEVISION)
            && pm.hasSystemFeature(PackageManager.FEATURE_LEANBACK);
  }

  private boolean isWifiEnabled() {
    final WifiManager wifiManager = mContext.getSystemService(WifiManager.class);
    return wifiManager.getWifiState() == WifiManager.WIFI_STATE_ENABLED
            || wifiManager.getWifiState() == WifiManager.WIFI_STATE_ENABLING;
  }

}
