/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.cts.appcompat;

import static com.google.common.truth.Truth.assertThat;

import android.Manifest;
import android.app.compat.CompatChanges;
import android.content.Context;
import android.os.Process;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Tests for the {@link android.app.compat.CompatChanges} SystemApi.
 *
 * These test methods have additional setup and post run checks done host side by
 * {@link com.android.cts.appcompat.CompatChangesSystemApiTest}.
 *
 * The setup adds an override for the change id being tested, and the post run step checks if
 * that change id has been logged to statsd.
 */
@RunWith(AndroidJUnit4.class)
public final class CompatChangesTest {
  private static final long CTS_SYSTEM_API_CHANGEID = 149391281;
  @Before
  public void setUp() {
    InstrumentationRegistry.getInstrumentation().getUiAutomation()
      .adoptShellPermissionIdentity(Manifest.permission.LOG_COMPAT_CHANGE,
                                    Manifest.permission.READ_COMPAT_CHANGE_CONFIG);
  }

  @After
  public void tearDown() {
    InstrumentationRegistry.getInstrumentation().getUiAutomation().dropShellPermissionIdentity();
  }

  /* Test run by CompatChangesSystemApiTest.testIsChangeEnabled */
  @Test
  public void isChangeEnabled_changeEnabled() {
    assertThat(CompatChanges.isChangeEnabled(CTS_SYSTEM_API_CHANGEID)).isTrue();
  }

  /* Test run by CompatChangesSystemApiTest.testIsChangeEnabledPackageName */
  @Test
  public void isChangeEnabledPackageName_changeEnabled() {
    Context context = InstrumentationRegistry.getTargetContext();
    assertThat(CompatChanges.isChangeEnabled(CTS_SYSTEM_API_CHANGEID, context.getPackageName(),
            context.getUser())).isTrue();
  }

  /* Test run by CompatChangesSystemApiTest.testIsChangeEnabledUid */
  @Test
  public void isChangeEnabledUid_changeEnabled() {
    assertThat(CompatChanges.isChangeEnabled(CTS_SYSTEM_API_CHANGEID, Process.myUid())).isTrue();
  }

  /* Test run by CompatChangesSystemApiTest.testIsChangeDisabled */
  @Test
  public void isChangeEnabled_changeDisabled() {
    assertThat(CompatChanges.isChangeEnabled(CTS_SYSTEM_API_CHANGEID)).isFalse();
  }

  /* Test run by CompatChangesSystemApiTest.testIsChangeDisabledPackageName */
  @Test
  public void isChangeEnabledPackageName_changeDisabled() {
    Context context = InstrumentationRegistry.getTargetContext();
    assertThat(CompatChanges.isChangeEnabled(CTS_SYSTEM_API_CHANGEID, context.getPackageName(),
            context.getUser())).isFalse();
  }

  /* Test run by CompatChangesSystemApiTest.testIsChangeDisabledUid */
  @Test
  public void isChangeEnabledUid_changeDisabled() {
    assertThat(CompatChanges.isChangeEnabled(CTS_SYSTEM_API_CHANGEID, Process.myUid())).isFalse();
  }
}
