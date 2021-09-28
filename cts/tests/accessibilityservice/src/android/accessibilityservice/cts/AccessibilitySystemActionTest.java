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

package android.accessibilityservice.cts;

import static android.accessibilityservice.cts.utils.AsyncUtils.DEFAULT_TIMEOUT_MS;
import static android.app.UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES;
import static org.junit.Assert.fail;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.AccessibilityService;
import android.app.Instrumentation;
import android.app.PendingIntent;
import android.app.RemoteAction;
import android.app.UiAutomation;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.drawable.Icon;
import android.platform.test.annotations.AppModeFull;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public class AccessibilitySystemActionTest {
    // intent actions to trigger system action callbacks
    private final static String INTENT_ACTION_SYSTEM_ACTION_CALLBACK_OVERRIDE_BACK = "android.accessibility.cts.end2endtests.action.system_action_callback_override_back";
    private final static String INTENT_ACTION_SYSTEM_ACTION_CALLBACK_NEW = "android.accessibility.cts.end2endtests.action.system_action_callback_new";

    private final static int NEW_ACTION_ID = 111;
    private final static String MANAGE_ACCESSIBILITY_PERMISSION = "android.permission.MANAGE_ACCESSIBILITY";

    private static Instrumentation sInstrumentation;
    private static UiAutomation sUiAutomation;

    private Context mContext;
    private AccessibilityManager mAccessibilityManager;

    private InstrumentedAccessibilityServiceTestRule<StubSystemActionsAccessibilityService>
            mServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
            StubSystemActionsAccessibilityService.class, false);

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mServiceRule)
            .around(mDumpOnFailureRule);

    StubSystemActionsAccessibilityService mService;

    @BeforeClass
    public static void oneTimeSetup() {
        sInstrumentation = InstrumentationRegistry.getInstrumentation();
        sUiAutomation = sInstrumentation.getUiAutomation(FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
        sUiAutomation.adoptShellPermissionIdentity(MANAGE_ACCESSIBILITY_PERMISSION);
    }

    @AfterClass
    public static void finalTearDown() {
        sUiAutomation.dropShellPermissionIdentity();
        sUiAutomation.destroy();
    }

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation().getContext();
        mAccessibilityManager =
                (AccessibilityManager) mContext.getSystemService(Context.ACCESSIBILITY_SERVICE);
        // Start stub accessibility service.
        mService = mServiceRule.enableService();
    }

    @After
    public void tearDown() throws Exception {
        mService.setLatch(null);
    }

    @Test
    @AppModeFull
    public void testRegisterOverriddenLegacyAction() {
        assertRegisterAction(AccessibilityService.GLOBAL_ACTION_BACK, null);
    }

    @Test
    @AppModeFull
    public void testUnregisterAction() {
        assertRegisterAction(AccessibilityService.GLOBAL_ACTION_BACK, null);
        assertUnregisterAction(AccessibilityService.GLOBAL_ACTION_BACK);
    }

    @Test
    @AppModeFull
    public void testNewActionInGetSystemActions() {
        assertRegisterAction(NEW_ACTION_ID, null);
        if (!mService.getSystemActions().contains(
                new AccessibilityAction(NEW_ACTION_ID, null))) {
            fail("new action should be in getSystemActions() list");
        }
    }


    @Test
    @AppModeFull
    public void testNewActionNotInGetSystemActions() {
        assertRegisterAction(NEW_ACTION_ID, null);
        assertUnregisterAction(NEW_ACTION_ID);
        if (mService.getSystemActions().contains(
                new AccessibilityAction(NEW_ACTION_ID, null))) {
            fail("new action should not be in getSystemActions() list");
        }
    }

    @Test
    @AppModeFull
    public void testPerformOverriddenLegacyAction() {
        assertRegisterAction(
                AccessibilityService.GLOBAL_ACTION_BACK,
                INTENT_ACTION_SYSTEM_ACTION_CALLBACK_OVERRIDE_BACK);
        assertPerformGlobalAction(
                AccessibilityService.GLOBAL_ACTION_BACK,
                INTENT_ACTION_SYSTEM_ACTION_CALLBACK_OVERRIDE_BACK);
    }

    @Test
    @AppModeFull
    public void testPerformNewAction() {
        assertRegisterAction(NEW_ACTION_ID, INTENT_ACTION_SYSTEM_ACTION_CALLBACK_NEW);
        assertPerformGlobalAction(NEW_ACTION_ID, INTENT_ACTION_SYSTEM_ACTION_CALLBACK_NEW);
    }

    private void assertRegisterAction(int actionID, String pendingIntent) {
        final CountDownLatch latch = new CountDownLatch(1);
        mService.setLatch(latch);
        try {
            RemoteAction r = getRemoteAction(pendingIntent);
            mAccessibilityManager.registerSystemAction(r, actionID);
            if (!latch.await(DEFAULT_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                fail("did not register " + actionID);
            }
        } catch (InterruptedException e) {
            fail("latch.await throws exception");
        }
    }

    private void assertUnregisterAction(int actionID) {
        final CountDownLatch latch = new CountDownLatch(1);
        mService.setLatch(latch);
        try {
            mAccessibilityManager.unregisterSystemAction(actionID);
            if (!latch.await(DEFAULT_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                fail("did not unregister " + actionID);
            }
        } catch (InterruptedException e) {
            fail("latch.await throws exception");
        }
    }

    private void assertPerformGlobalAction(int actionId, String pendingIntent) {
        final CountDownLatch receiverLatch = new CountDownLatch(1);
        BroadcastReceiver receiver = new SystemActionBroadcastReceiver(
                receiverLatch,
                pendingIntent);
        mContext.registerReceiver(receiver, new IntentFilter(pendingIntent));
        try {
            mService.performGlobalAction(actionId);
            if (!receiverLatch.await(DEFAULT_TIMEOUT_MS, TimeUnit.MILLISECONDS)) {
                fail("action not triggered; did not receive callback intent");
            }
        } catch (InterruptedException e) {
            fail("latch.await throws exception");
        } finally {
            mContext.unregisterReceiver(receiver);
            mAccessibilityManager.unregisterSystemAction(actionId);
        }
    }

    private RemoteAction getRemoteAction(String pendingIntent) {
        Intent i = new Intent(pendingIntent);
        PendingIntent p = PendingIntent.getBroadcast(mContext, 0, i, 0);
        return new RemoteAction(Icon.createWithContentUri("content://test"), "test1", "test1", p);
    }

    private static class SystemActionBroadcastReceiver extends BroadcastReceiver {
        private final CountDownLatch mLatch;
        private final String mExpectedAction;

        public SystemActionBroadcastReceiver(CountDownLatch latch, String expectedAction) {
            super();
            mLatch = latch;
            mExpectedAction = expectedAction;
        }

        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (mExpectedAction.equals(action)) {
                mLatch.countDown();
            }
        }
    }
}
