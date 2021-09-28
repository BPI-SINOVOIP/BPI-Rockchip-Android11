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
 * limitations under the License
 */
package android.attentionservice.cts;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import static com.android.compatibility.common.util.ShellUtils.runShellCommand;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assume.assumeTrue;

import android.platform.test.annotations.AppModeFull;
import android.provider.DeviceConfig;
import android.service.attention.AttentionService;
import android.text.TextUtils;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.compatibility.common.util.DeviceConfigStateChangerRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * This suite of test ensures that AttentionManagerService behaves correctly when properly bound
 * to an AttentionService implementation
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull(reason = "PM will not recognize CtsTestAttentionService in instantMode.")
public class CtsAttentionServiceDeviceTest {
    private static final String SERVICE_ENABLED = "service_enabled";
    private static final String FAKE_SERVICE_PACKAGE =
            CtsTestAttentionService.class.getPackage().getName();
    private final boolean isTestable =
            !TextUtils.isEmpty(getAttentionServiceComponent());

    @Rule
    public final DeviceConfigStateChangerRule mLookAllTheseRules =
            new DeviceConfigStateChangerRule(getInstrumentation().getTargetContext(),
                    DeviceConfig.NAMESPACE_ATTENTION_MANAGER_SERVICE,
                    SERVICE_ENABLED,
                    "true");

    @Before
    public void setUp() {
        assumeTrue("Feature not available on this device. Skipping test.", isTestable);
        clearTestableAttentionService();
        CtsTestAttentionService.reset();
        bindToTestService();
    }

    @After
    public void tearDown() {
        clearTestableAttentionService();
    }

    @Test
    public void testAttentionService_OnSuccess() {
        /** From manager, call CheckAttention() on test service */
        assertThat(CtsTestAttentionService.hasPendingChecks()).isFalse();
        callCheckAttention();

        /** From test service, verify that onCheckAttention was called */
        assertThat(CtsTestAttentionService.hasPendingChecks()).isTrue();

        /** From test service, respond with onSuccess */
        CtsTestAttentionService.respondSuccess(
                AttentionService.ATTENTION_SUCCESS_PRESENT);

        /** From manager, verify onSuccess callback was received*/
        assertThat(getLastTestCallbackCode()).isEqualTo(AttentionService.ATTENTION_SUCCESS_PRESENT);
    }

    @Test
    public void testAttentionService_OnCancelledFromManager() {
        /** From manager, call CheckAttention() on test service */
        assertThat(CtsTestAttentionService.hasPendingChecks()).isFalse();
        callCheckAttention();

        /** From test service, verify that onCheckAttention was called */
        assertThat(CtsTestAttentionService.hasPendingChecks()).isTrue();

        /** From manager, cancel the check */
        callCancelAttention();

        /** From test service, verify that the check was cancelled */
        assertThat(CtsTestAttentionService.hasPendingChecks()).isFalse();

        /** From manager, verify that the onFailure callback was called with
         * ATTENTION_FAILURE_CANCELLED */
        assertThat(getLastTestCallbackCode()).isEqualTo(
                AttentionService.ATTENTION_FAILURE_CANCELLED);
    }

    @Test
    public void testAttentionService_OnCancelledFromService() {
        /** From manager, call CheckAttention() on test service */
        assertThat(CtsTestAttentionService.hasPendingChecks()).isFalse();
        callCheckAttention();

        /** From test service, verify that onCheckAttention was called */
        assertThat(CtsTestAttentionService.hasPendingChecks()).isTrue();

        /** From test service, cancel the check and respond with ATTENTION_FAILURE_CANCELLED */
        CtsTestAttentionService.respondFailure(AttentionService.ATTENTION_FAILURE_CANCELLED);

        /** From test service, verify that the check was cancelled */
        assertThat(CtsTestAttentionService.hasPendingChecks()).isFalse();

        /** From manager, verify that the onFailure callback was called with
         * ATTENTION_FAILURE_CANCELLED */
        assertThat(getLastTestCallbackCode()).isEqualTo(
                AttentionService.ATTENTION_FAILURE_CANCELLED);
    }

    private void bindToTestService() {
        /** On Manager, bind to test service */
        assertThat(getAttentionServiceComponent()).isNotEqualTo(FAKE_SERVICE_PACKAGE);
        assertThat(setTestableAttentionService(FAKE_SERVICE_PACKAGE)).isTrue();
        assertThat(getAttentionServiceComponent()).contains(FAKE_SERVICE_PACKAGE);
    }

    private String getAttentionServiceComponent() {
        return runShellCommand("cmd attention getAttentionServiceComponent");
    }

    private int getLastTestCallbackCode() {
        return Integer.parseInt(runShellCommand("cmd attention getLastTestCallbackCode"));
    }

    /**
     * This call is asynchronous (manager spawns + binds to service and then asynchronously makes a
     * check attention call).
     * As such, we need to ensure consistent testing results, by waiting until we receive a response
     * in our test service w/ CountDownLatch(s).
     */
    private void callCheckAttention() {
        wakeUpScreen();
        Boolean isSuccess = runShellCommand("cmd attention call checkAttention")
                .equals("true");
        assertThat(isSuccess).isTrue();
        CtsTestAttentionService.onReceivedResponse();
    }

    private void callCancelAttention() {
        runShellCommand("cmd attention call cancelCheckAttention");
        CtsTestAttentionService.onReceivedResponse();
    }

    private boolean setTestableAttentionService(String service) {
        return runShellCommand("cmd attention setTestableAttentionService " + service)
                .equals("true");
    }

    private void clearTestableAttentionService() {
        runShellCommand("cmd attention clearTestableAttentionService");
    }

    private void wakeUpScreen() {
        runShellCommand("input keyevent KEYCODE_WAKEUP");
    }
}
