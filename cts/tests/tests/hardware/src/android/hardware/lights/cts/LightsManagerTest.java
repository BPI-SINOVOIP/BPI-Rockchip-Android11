/*
 * Copyright 2020 The Android Open Source Project
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

package android.hardware.lights.cts.tests;

import static android.hardware.lights.LightsRequest.Builder;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.hardware.lights.Light;
import android.hardware.lights.LightState;
import android.hardware.lights.LightsManager;

import androidx.annotation.ColorInt;
import androidx.test.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

@RunWith(AndroidJUnit4.class)
@SmallTest
public class LightsManagerTest {

    private static final int ON_TAN = 0xffd2b48c;
    private static final int ON_RED = 0xffff0000;
    private static final LightState STATE_TAN = new LightState(ON_TAN);
    private static final LightState STATE_RED = new LightState(ON_RED);

    private LightsManager mManager;
    private List<Light> mLights;

    @Before
    public void setUp() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(
                        android.Manifest.permission.CONTROL_DEVICE_LIGHTS);

        final Context context = InstrumentationRegistry.getTargetContext();
        mManager = context.getSystemService(LightsManager.class);
        mLights = mManager.getLights();
    }

    @After
    public void tearDown() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
    }

    @Test
    public void testControlLightsPermissionIsRequiredToUseLights() {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
        try {
            mManager.getLights();
            fail("Expected SecurityException to be thrown for getLights()");
        } catch (SecurityException expected) {
        }

        try (LightsManager.LightsSession session = mManager.openSession()) {
            fail("Expected SecurityException to be thrown for openSession()");
        } catch (SecurityException expected) {
        }
    }

    @Test
    public void testControlSingleLight() {
        assumeTrue(mLights.size() >= 1);

        try (LightsManager.LightsSession session = mManager.openSession()) {
            // When the session requests to turn a single light on:
            session.requestLights(new Builder()
                    .setLight(mLights.get(0), STATE_RED)
                    .build());

            // Then the light should turn on.
            assertThat(mManager.getLightState(mLights.get(0)).getColor()).isEqualTo(ON_RED);
        }
    }

    @Test
    public void testControlMultipleLights() {
        assumeTrue(mLights.size() >= 2);

        try (LightsManager.LightsSession session = mManager.openSession()) {
            // When the session requests to turn two of the lights on:
            session.requestLights(new Builder()
                    .setLight(mLights.get(0), new LightState(0xffaaaaff))
                    .setLight(mLights.get(1), new LightState(0xffbbbbff))
                    .build());

            // Then both should turn on.
            assertThat(mManager.getLightState(mLights.get(0)).getColor()).isEqualTo(0xffaaaaff);
            assertThat(mManager.getLightState(mLights.get(1)).getColor()).isEqualTo(0xffbbbbff);

            // Any others should remain off.
            for (int i = 2; i < mLights.size(); i++) {
                assertThat(mManager.getLightState(mLights.get(i)).getColor()).isEqualTo(0x00);
            }
        }
    }

    @Test
    public void testControlLights_onlyEffectiveForLifetimeOfClient() {
        assumeTrue(mLights.size() >= 1);

        // The light should begin by being off.
        assertThat(mManager.getLightState(mLights.get(0)).getColor()).isEqualTo(0x00);

        try (LightsManager.LightsSession session = mManager.openSession()) {
            // When a session commits changes:
            session.requestLights(new Builder().setLight(mLights.get(0), STATE_TAN).build());
            // Then the light should turn on.
            assertThat(mManager.getLightState(mLights.get(0)).getColor()).isEqualTo(ON_TAN);

            // When the session goes away:
            session.close();
            // Then the light should turn off.
            assertThat(mManager.getLightState(mLights.get(0)).getColor()).isEqualTo(0x00);
        }
    }

    @Test
    public void testControlLights_firstCallerWinsContention() {
        assumeTrue(mLights.size() >= 1);

        try (LightsManager.LightsSession session1 = mManager.openSession();
                LightsManager.LightsSession session2 = mManager.openSession()) {

            // When session1 and session2 both request the same light:
            session1.requestLights(new Builder().setLight(mLights.get(0), STATE_TAN).build());
            session2.requestLights(new Builder().setLight(mLights.get(0), STATE_RED).build());
            // Then session1 should win because it was created first.
            assertThat(mManager.getLightState(mLights.get(0)).getColor()).isEqualTo(ON_TAN);

            // When session1 goes away:
            session1.close();
            // Then session2 should have its request go into effect.
            assertThat(mManager.getLightState(mLights.get(0)).getColor()).isEqualTo(ON_RED);

            // When session2 goes away:
            session2.close();
            // Then the light should turn off because there are no more sessions.
            assertThat(mManager.getLightState(mLights.get(0)).getColor()).isEqualTo(0);
        }
    }

    @Test
    public void testClearLight() {
        assumeTrue(mLights.size() >= 1);

        try (LightsManager.LightsSession session = mManager.openSession()) {
            // When the session turns a light on:
            session.requestLights(new Builder().setLight(mLights.get(0), STATE_RED).build());
            // And then the session clears it again:
            session.requestLights(new Builder().clearLight(mLights.get(0)).build());
            // Then the light should turn back off.
            assertThat(mManager.getLightState(mLights.get(0)).getColor()).isEqualTo(0);
        }
    }
}
