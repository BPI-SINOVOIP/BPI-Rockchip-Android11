/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.google.android.exoplayer2.playbacktests.gts;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;
import static org.junit.Assert.assertThrows;

import android.media.MediaDrm.MediaDrmStateException;
import android.net.Uri;
import android.util.Pair;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.rule.ActivityTestRule;
import com.google.android.exoplayer2.Player;
import com.google.android.exoplayer2.drm.DrmInitData;
import com.google.android.exoplayer2.drm.DrmSession.DrmSessionException;
import com.google.android.exoplayer2.drm.OfflineLicenseHelper;
import com.google.android.exoplayer2.source.dash.DashUtil;
import com.google.android.exoplayer2.source.dash.manifest.DashManifest;
import com.google.android.exoplayer2.testutil.ActionSchedule;
import com.google.android.exoplayer2.testutil.HostActivity;
import com.google.android.exoplayer2.upstream.DataSource;
import com.google.android.exoplayer2.upstream.DefaultHttpDataSourceFactory;
import com.google.android.exoplayer2.util.MediaSourceEventDispatcher;
import com.google.android.exoplayer2.util.MimeTypes;
import com.google.android.exoplayer2.util.Util;
import java.io.IOException;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Tests Widevine encrypted DASH playbacks using offline keys. */
@RunWith(AndroidJUnit4.class)
public final class DashWidevineOfflineTest {

  private static final String TAG = "DashWidevineOfflineTest";
  private static final String USER_AGENT = "ExoPlayerPlaybackTests";

  private DashTestRunner testRunner;
  private DefaultHttpDataSourceFactory httpDataSourceFactory;
  private OfflineLicenseHelper offlineLicenseHelper;
  private byte[] offlineLicenseKeySetId;

  @Rule public ActivityTestRule<HostActivity> testRule = new ActivityTestRule<>(HostActivity.class);

  @Before
  public void setUp() throws Exception {
    testRunner =
        new DashTestRunner(TAG, testRule.getActivity())
            .setStreamName("test_widevine_h264_fixed_offline")
            .setManifestUrl(DashTestData.WIDEVINE_H264_MANIFEST)
            .setWidevineInfo(MimeTypes.VIDEO_H264, true)
            .setFullPlaybackNoSeeking(true)
            .setCanIncludeAdditionalVideoFormats(false)
            .setAudioVideoFormats(
                DashTestData.WIDEVINE_AAC_AUDIO_REPRESENTATION_ID,
                DashTestData.WIDEVINE_H264_CDD_FIXED);

    boolean useL1Widevine = DashTestRunner.isL1WidevineAvailable(MimeTypes.VIDEO_H264);
    String widevineLicenseUrl = DashTestData.getWidevineLicenseUrl(true, useL1Widevine);
    httpDataSourceFactory = new DefaultHttpDataSourceFactory(USER_AGENT);
    if (Util.SDK_INT >= 18) {
      offlineLicenseHelper =
          OfflineLicenseHelper.newWidevineInstance(
              widevineLicenseUrl, httpDataSourceFactory, new MediaSourceEventDispatcher());
    }
  }

  @After
  public void tearDown() throws Exception {
    testRunner = null;
    if (offlineLicenseKeySetId != null) {
      releaseLicense();
    }
    if (offlineLicenseHelper != null) {
      offlineLicenseHelper.release();
    }
    offlineLicenseHelper = null;
    httpDataSourceFactory = null;
  }

  // Offline license tests

  @Test
  public void widevineOfflineLicenseV22() throws Exception {
    if (Util.SDK_INT < 22) {
      return; // Pass.
    }
    downloadLicense();
    testRunner.run();

    // Renew license after playback should still work
    offlineLicenseKeySetId = offlineLicenseHelper.renewLicense(offlineLicenseKeySetId);
    assertThat(offlineLicenseKeySetId).isNotNull();
  }

  @Test
  public void widevineOfflineReleasedLicenseV22() throws Throwable {
    if (Util.SDK_INT < 22 || Util.SDK_INT > 28) {
      return; // Pass.
    }
    downloadLicense();
    releaseLicense(); // keySetId no longer valid.

    Throwable error =
        assertThrows(
            "Playback should fail because the license has been released.",
            Throwable.class,
            () -> testRunner.run());

    // Get the root cause
    Throwable cause = error.getCause();
    while (cause != null && cause != error) {
      error = cause;
      cause = error.getCause();
    }
    assertThat(error).isInstanceOf(MediaDrmStateException.class);
  }

  @Test
  public void widevineOfflineReleasedLicenseV29() throws Throwable {
    if (Util.SDK_INT < 29) {
      return; // Pass.
    }
    downloadLicense();
    releaseLicense(); // keySetId no longer valid.

    Throwable error =
        assertThrows(
            "Playback should fail because the license has been released.",
            Throwable.class,
            () -> testRunner.run());
    // Get the root cause
    Throwable cause = error.getCause();
    while (cause != null && cause != error) {
      error = cause;
      cause = error.getCause();
    }
    assertThat(error).isInstanceOf(IllegalArgumentException.class);
  }

  @Test
  public void widevineOfflineExpiredLicenseV22() throws Exception {
    if (Util.SDK_INT < 22) {
      return; // Pass.
    }
    downloadLicense();

    // Wait until the license expires
    long licenseDuration =
        offlineLicenseHelper.getLicenseDurationRemainingSec(offlineLicenseKeySetId).first;
    assertWithMessage(
            "License duration should be less than 30 sec. " + "Server settings might have changed.")
        .that(licenseDuration < 30)
        .isTrue();
    while (licenseDuration > 0) {
      synchronized (this) {
        wait(licenseDuration * 1000 + 2000);
      }
      long previousDuration = licenseDuration;
      licenseDuration =
          offlineLicenseHelper.getLicenseDurationRemainingSec(offlineLicenseKeySetId).first;
      assertWithMessage("License duration should be decreasing.")
          .that(previousDuration > licenseDuration)
          .isTrue();
    }

    // DefaultDrmSessionManager should renew the license and stream play fine
    testRunner.run();
  }

  @Test
  public void widevineOfflineLicenseExpiresOnPauseV22() throws Exception {
    if (Util.SDK_INT < 22) {
      return; // Pass.
    }
    downloadLicense();

    // During playback pause until the license expires then continue playback
    Pair<Long, Long> licenseDurationRemainingSec =
        offlineLicenseHelper.getLicenseDurationRemainingSec(offlineLicenseKeySetId);
    long licenseDuration = licenseDurationRemainingSec.first;
    assertWithMessage(
            "License duration should be less than 30 sec. " + "Server settings might have changed.")
        .that(licenseDuration < 30)
        .isTrue();
    ActionSchedule schedule = new ActionSchedule.Builder(TAG)
        .waitForPlaybackState(Player.STATE_READY)
        .delay(3000).pause().delay(licenseDuration * 1000 + 2000).play().build();

    // DefaultDrmSessionManager should renew the license and stream play fine
    testRunner.setActionSchedule(schedule).run();
  }

  private void downloadLicense() throws IOException {
    DataSource dataSource = httpDataSourceFactory.createDataSource();
    DashManifest dashManifest = DashUtil.loadManifest(dataSource,
        Uri.parse(DashTestData.WIDEVINE_H264_MANIFEST));
    DrmInitData drmInitData = DashUtil.loadDrmInitData(dataSource, dashManifest.getPeriod(0));
    offlineLicenseKeySetId = offlineLicenseHelper.downloadLicense(drmInitData);
    assertThat(offlineLicenseKeySetId).isNotNull();
    assertThat(offlineLicenseKeySetId.length).isGreaterThan(0);
    testRunner.setOfflineLicenseKeySetId(offlineLicenseKeySetId);
  }

  private void releaseLicense() throws DrmSessionException {
    offlineLicenseHelper.releaseLicense(offlineLicenseKeySetId);
    offlineLicenseKeySetId = null;
  }

}
