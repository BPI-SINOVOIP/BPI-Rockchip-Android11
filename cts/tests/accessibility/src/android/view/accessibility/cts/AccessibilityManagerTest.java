/*
 * Copyright (C) 2012 The Android Open Source Project
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

package android.view.accessibility.cts;

import static android.accessibility.cts.common.InstrumentedAccessibilityService.TIMEOUT_SERVICE_ENABLE;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityService;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.accessibilityservice.AccessibilityServiceInfo;
import android.app.Instrumentation;
import android.app.Service;
import android.app.UiAutomation;
import android.content.Context;
import android.content.pm.ServiceInfo;
import android.os.Handler;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityManager.AccessibilityStateChangeListener;
import android.view.accessibility.AccessibilityManager.TouchExplorationStateChangeListener;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.PollingCheck;
import com.android.compatibility.common.util.SystemUtil;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.io.IOException;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Class for testing {@link AccessibilityManager}.
 */
@RunWith(AndroidJUnit4.class)
public class AccessibilityManagerTest {

    private AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    private InstrumentedAccessibilityServiceTestRule<SpeakingAccessibilityService>
            mSpeakingAccessibilityServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
                    SpeakingAccessibilityService.class, false);

    private InstrumentedAccessibilityServiceTestRule<VibratingAccessibilityService>
            mVibratingAccessibilityServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
                    VibratingAccessibilityService.class, false);

    private InstrumentedAccessibilityServiceTestRule<SpeakingAndVibratingAccessibilityService>
            mSpeakingAndVibratingAccessibilityServiceRule =
            new InstrumentedAccessibilityServiceTestRule<>(
                    SpeakingAndVibratingAccessibilityService.class, false);

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mSpeakingAndVibratingAccessibilityServiceRule)
            .around(mVibratingAccessibilityServiceRule)
            .around(mSpeakingAccessibilityServiceRule)
            // Inner rule capture failure and dump data before finishing activity and a11y service
            .around(mDumpOnFailureRule);

    private static final Instrumentation sInstrumentation =
            InstrumentationRegistry.getInstrumentation();

    private static final String SPEAKING_ACCESSIBLITY_SERVICE_NAME =
        "android.view.accessibility.cts.SpeakingAccessibilityService";

    private static final String VIBRATING_ACCESSIBLITY_SERVICE_NAME =
        "android.view.accessibility.cts.VibratingAccessibilityService";

    private static final String MULTIPLE_FEEDBACK_TYPES_ACCESSIBILITY_SERVICE_NAME =
        "android.view.accessibility.cts.SpeakingAndVibratingAccessibilityService";

    public static final String ACCESSIBILITY_NON_INTERACTIVE_UI_TIMEOUT_MS =
            "accessibility_non_interactive_ui_timeout_ms";

    public static final String ACCESSIBILITY_INTERACTIVE_UI_TIMEOUT_MS =
            "accessibility_interactive_ui_timeout_ms";

    private AccessibilityManager mAccessibilityManager;

    private Context mTargetContext;

    private Handler mHandler;

    @Before
    public void setUp() throws Exception {
        mAccessibilityManager = (AccessibilityManager)
                sInstrumentation.getContext().getSystemService(Service.ACCESSIBILITY_SERVICE);
        mTargetContext = sInstrumentation.getTargetContext();
        mHandler = new Handler(mTargetContext.getMainLooper());
        // In case the test runner started a UiAutomation, destroy it to start with a clean slate.
        sInstrumentation.getUiAutomation().destroy();
        InstrumentedAccessibilityService.disableAllServices();
    }

    @Test
    public void testAddAndRemoveAccessibilityStateChangeListener() throws Exception {
        AccessibilityStateChangeListener listener = (state) -> {
                /* do nothing */
        };
        assertTrue(mAccessibilityManager.addAccessibilityStateChangeListener(listener));
        assertTrue(mAccessibilityManager.removeAccessibilityStateChangeListener(listener));
        assertFalse(mAccessibilityManager.removeAccessibilityStateChangeListener(listener));
    }

    @Test
    public void testAddAndRemoveTouchExplorationStateChangeListener() throws Exception {
        TouchExplorationStateChangeListener listener = (boolean enabled) -> {
            // Do nothing.
        };
        assertTrue(mAccessibilityManager.addTouchExplorationStateChangeListener(listener));
        assertTrue(mAccessibilityManager.removeTouchExplorationStateChangeListener(listener));
        assertFalse(mAccessibilityManager.removeTouchExplorationStateChangeListener(listener));
    }

    @Test
    public void testIsTouchExplorationEnabled() throws Exception {
        mSpeakingAccessibilityServiceRule.enableService();
        mVibratingAccessibilityServiceRule.enableService();
        new PollingCheck() {
            @Override
            protected boolean check() {
                return mAccessibilityManager.isTouchExplorationEnabled();
            }
        }.run();
    }

    @Test
    public void testGetInstalledAccessibilityServicesList() throws Exception {
        List<AccessibilityServiceInfo> installedServices =
            mAccessibilityManager.getInstalledAccessibilityServiceList();
        assertFalse("There must be at least one installed service.", installedServices.isEmpty());
        boolean speakingServiceInstalled = false;
        boolean vibratingServiceInstalled = false;
        final int serviceCount = installedServices.size();
        for (int i = 0; i < serviceCount; i++) {
            AccessibilityServiceInfo installedService = installedServices.get(i);
            ServiceInfo serviceInfo = installedService.getResolveInfo().serviceInfo;
            if (mTargetContext.getPackageName().equals(serviceInfo.packageName)
                    && SPEAKING_ACCESSIBLITY_SERVICE_NAME.equals(serviceInfo.name)) {
                speakingServiceInstalled = true;
            }
            if (mTargetContext.getPackageName().equals(serviceInfo.packageName)
                    && VIBRATING_ACCESSIBLITY_SERVICE_NAME.equals(serviceInfo.name)) {
                vibratingServiceInstalled = true;
            }
        }
        assertTrue("The speaking service should be installed.", speakingServiceInstalled);
        assertTrue("The vibrating service should be installed.", vibratingServiceInstalled);
    }

    @Test
    public void testGetEnabledAccessibilityServiceList() throws Exception {
        mSpeakingAccessibilityServiceRule.enableService();
        mVibratingAccessibilityServiceRule.enableService();
        List<AccessibilityServiceInfo> enabledServices =
            mAccessibilityManager.getEnabledAccessibilityServiceList(
                    AccessibilityServiceInfo.FEEDBACK_ALL_MASK);
        boolean speakingServiceEnabled = false;
        boolean vibratingServiceEnabled = false;
        final int serviceCount = enabledServices.size();
        for (int i = 0; i < serviceCount; i++) {
            AccessibilityServiceInfo enabledService = enabledServices.get(i);
            ServiceInfo serviceInfo = enabledService.getResolveInfo().serviceInfo;
            if (mTargetContext.getPackageName().equals(serviceInfo.packageName)
                    && SPEAKING_ACCESSIBLITY_SERVICE_NAME.equals(serviceInfo.name)) {
                speakingServiceEnabled = true;
            }
            if (mTargetContext.getPackageName().equals(serviceInfo.packageName)
                    && VIBRATING_ACCESSIBLITY_SERVICE_NAME.equals(serviceInfo.name)) {
                vibratingServiceEnabled = true;
            }
        }
        assertTrue("The speaking service should be enabled.", speakingServiceEnabled);
        assertTrue("The vibrating service should be enabled.", vibratingServiceEnabled);
    }

    @Test
    public void testGetEnabledAccessibilityServiceListForType() throws Exception {
        mSpeakingAccessibilityServiceRule.enableService();
        mVibratingAccessibilityServiceRule.enableService();
        List<AccessibilityServiceInfo> enabledServices =
            mAccessibilityManager.getEnabledAccessibilityServiceList(
                    AccessibilityServiceInfo.FEEDBACK_SPOKEN);
        assertSame("There should be only one enabled speaking service.", 1, enabledServices.size());
        final int serviceCount = enabledServices.size();
        for (int i = 0; i < serviceCount; i++) {
            AccessibilityServiceInfo enabledService = enabledServices.get(i);
            ServiceInfo serviceInfo = enabledService.getResolveInfo().serviceInfo;
            if (mTargetContext.getPackageName().equals(serviceInfo.packageName)
                    && SPEAKING_ACCESSIBLITY_SERVICE_NAME.equals(serviceInfo.name)) {
                return;
            }
        }
        fail("The speaking service is not enabled.");
    }

    @Test
    public void testGetEnabledAccessibilityServiceListForTypes() throws Exception {
        mSpeakingAccessibilityServiceRule.enableService();
        mVibratingAccessibilityServiceRule.enableService();
        // For this test, also enable a service with multiple feedback types
        mSpeakingAndVibratingAccessibilityServiceRule.enableService();

        List<AccessibilityServiceInfo> enabledServices =
                mAccessibilityManager.getEnabledAccessibilityServiceList(
                        AccessibilityServiceInfo.FEEDBACK_SPOKEN
                                | AccessibilityServiceInfo.FEEDBACK_HAPTIC);
        assertSame("There should be 3 enabled accessibility services.", 3, enabledServices.size());
        boolean speakingServiceEnabled = false;
        boolean vibratingServiceEnabled = false;
        boolean multipleFeedbackTypesServiceEnabled = false;
        final int serviceCount = enabledServices.size();
        for (int i = 0; i < serviceCount; i++) {
            AccessibilityServiceInfo enabledService = enabledServices.get(i);
            ServiceInfo serviceInfo = enabledService.getResolveInfo().serviceInfo;
            if (mTargetContext.getPackageName().equals(serviceInfo.packageName)
                    && SPEAKING_ACCESSIBLITY_SERVICE_NAME.equals(serviceInfo.name)) {
                speakingServiceEnabled = true;
            }
            if (mTargetContext.getPackageName().equals(serviceInfo.packageName)
                    && VIBRATING_ACCESSIBLITY_SERVICE_NAME.equals(serviceInfo.name)) {
                vibratingServiceEnabled = true;
            }
            if (mTargetContext.getPackageName().equals(serviceInfo.packageName)
                    && MULTIPLE_FEEDBACK_TYPES_ACCESSIBILITY_SERVICE_NAME.equals(
                    serviceInfo.name)) {
                multipleFeedbackTypesServiceEnabled = true;
            }
        }
        assertTrue("The speaking service should be enabled.", speakingServiceEnabled);
        assertTrue("The vibrating service should be enabled.", vibratingServiceEnabled);
        assertTrue("The multiple feedback types service should be enabled.",
                multipleFeedbackTypesServiceEnabled);
    }

    @SuppressWarnings("deprecation")
    @Test
    public void testGetAccessibilityServiceList() throws Exception {
        List<ServiceInfo> services = mAccessibilityManager.getAccessibilityServiceList();
        boolean speakingServiceInstalled = false;
        boolean vibratingServiceInstalled = false;
        final int serviceCount = services.size();
        for (int i = 0; i < serviceCount; i++) {
            ServiceInfo serviceInfo = services.get(i);
            if (mTargetContext.getPackageName().equals(serviceInfo.packageName)
                    && SPEAKING_ACCESSIBLITY_SERVICE_NAME.equals(serviceInfo.name)) {
                speakingServiceInstalled = true;
            }
            if (mTargetContext.getPackageName().equals(serviceInfo.packageName)
                    && VIBRATING_ACCESSIBLITY_SERVICE_NAME.equals(serviceInfo.name)) {
                vibratingServiceInstalled = true;
            }
        }
        assertTrue("The speaking service should be installed.", speakingServiceInstalled);
        assertTrue("The vibrating service should be installed.", vibratingServiceInstalled);
    }

    @Test
    public void testInterrupt() throws Exception {
        // The APIs are heavily tested in the android.accessibilityservice package.
        // This just makes sure the call does not throw an exception.
        mSpeakingAccessibilityServiceRule.enableService();
        mVibratingAccessibilityServiceRule.enableService();
        waitForAccessibilityEnabled();
        mAccessibilityManager.interrupt();
    }

    @Test
    public void testSendAccessibilityEvent() throws Exception {
        // The APIs are heavily tested in the android.accessibilityservice package.
        // This just makes sure the call does not throw an exception.
        mSpeakingAccessibilityServiceRule.enableService();
        mVibratingAccessibilityServiceRule.enableService();
        waitForAccessibilityEnabled();
        mAccessibilityManager.sendAccessibilityEvent(AccessibilityEvent.obtain(
                AccessibilityEvent.TYPE_VIEW_CLICKED));
    }

    @Test
    public void testTouchExplorationListenerNoHandler() throws Exception {
        final Object waitObject = new Object();
        final AtomicBoolean atomicBoolean = new AtomicBoolean(false);

        TouchExplorationStateChangeListener listener = (boolean b) -> {
            synchronized (waitObject) {
                atomicBoolean.set(b);
                waitObject.notifyAll();
            }
        };
        mAccessibilityManager.addTouchExplorationStateChangeListener(listener);
        mSpeakingAccessibilityServiceRule.enableService();
        mVibratingAccessibilityServiceRule.enableService();
        assertAtomicBooleanBecomes(atomicBoolean, true, waitObject,
                "Touch exploration state listener not called when services enabled");
        assertTrue("Listener told that touch exploration is enabled, but manager says disabled",
                mAccessibilityManager.isTouchExplorationEnabled());
        InstrumentedAccessibilityService.disableAllServices();
        assertAtomicBooleanBecomes(atomicBoolean, false, waitObject,
                "Touch exploration state listener not called when services disabled");
        assertFalse("Listener told that touch exploration is disabled, but manager says it enabled",
                mAccessibilityManager.isTouchExplorationEnabled());
        mAccessibilityManager.removeTouchExplorationStateChangeListener(listener);
    }

    @Test
    public void testTouchExplorationListenerWithHandler() throws Exception {
        final Object waitObject = new Object();
        final AtomicBoolean atomicBoolean = new AtomicBoolean(false);

        TouchExplorationStateChangeListener listener = (boolean b) -> {
            synchronized (waitObject) {
                atomicBoolean.set(b);
                waitObject.notifyAll();
            }
        };
        mAccessibilityManager.addTouchExplorationStateChangeListener(listener, mHandler);
        mSpeakingAccessibilityServiceRule.enableService();
        mVibratingAccessibilityServiceRule.enableService();
        assertAtomicBooleanBecomes(atomicBoolean, true, waitObject,
                "Touch exploration state listener not called when services enabled");
        assertTrue("Listener told that touch exploration is enabled, but manager says disabled",
                mAccessibilityManager.isTouchExplorationEnabled());
        InstrumentedAccessibilityService.disableAllServices();
        assertAtomicBooleanBecomes(atomicBoolean, false, waitObject,
                "Touch exploration state listener not called when services disabled");
        assertFalse("Listener told that touch exploration is disabled, but manager says it enabled",
                mAccessibilityManager.isTouchExplorationEnabled());
        mAccessibilityManager.removeTouchExplorationStateChangeListener(listener);
    }

    @Test
    public void testAccessibilityStateListenerNoHandler() throws Exception {
        final Object waitObject = new Object();
        final AtomicBoolean atomicBoolean = new AtomicBoolean(false);

        AccessibilityStateChangeListener listener = (boolean b) -> {
            synchronized (waitObject) {
                atomicBoolean.set(b);
                waitObject.notifyAll();
            }
        };
        mAccessibilityManager.addAccessibilityStateChangeListener(listener);
        mSpeakingAndVibratingAccessibilityServiceRule.enableService();
        assertAtomicBooleanBecomes(atomicBoolean, true, waitObject,
                "Accessibility state listener not called when services enabled");
        assertTrue("Listener told that accessibility is enabled, but manager says disabled",
                mAccessibilityManager.isEnabled());
        InstrumentedAccessibilityService.disableAllServices();
        assertAtomicBooleanBecomes(atomicBoolean, false, waitObject,
                "Accessibility state listener not called when services disabled");
        assertFalse("Listener told that accessibility is disabled, but manager says enabled",
                mAccessibilityManager.isEnabled());
        mAccessibilityManager.removeAccessibilityStateChangeListener(listener);
    }

    @Test
    public void testAccessibilityStateListenerWithHandler() throws Exception {
        final Object waitObject = new Object();
        final AtomicBoolean atomicBoolean = new AtomicBoolean(false);

        AccessibilityStateChangeListener listener = (boolean b) -> {
            synchronized (waitObject) {
                atomicBoolean.set(b);
                waitObject.notifyAll();
            }
        };
        mAccessibilityManager.addAccessibilityStateChangeListener(listener, mHandler);
        mSpeakingAndVibratingAccessibilityServiceRule.enableService();
        assertAtomicBooleanBecomes(atomicBoolean, true, waitObject,
                "Accessibility state listener not called when services enabled");
        assertTrue("Listener told that accessibility is enabled, but manager says disabled",
                mAccessibilityManager.isEnabled());
        InstrumentedAccessibilityService.disableAllServices();
        assertAtomicBooleanBecomes(atomicBoolean, false, waitObject,
                "Accessibility state listener not called when services disabled");
        assertFalse("Listener told that accessibility is disabled, but manager says enabled",
                mAccessibilityManager.isEnabled());
        mAccessibilityManager.removeAccessibilityStateChangeListener(listener);
    }

    @Test
    public void testGetRecommendedTimeoutMillis() throws Exception {
        mSpeakingAccessibilityServiceRule.enableService();
        mVibratingAccessibilityServiceRule.enableService();
        waitForAccessibilityEnabled();
        UiAutomation automan = sInstrumentation.getUiAutomation(
                UiAutomation.FLAG_DONT_SUPPRESS_ACCESSIBILITY_SERVICES);
        try {
            // SpeakingA11yService interactive/nonInteractive timeout is 6000/1000
            // vibratingA11yService interactive/nonInteractive timeout is 5000/2000
            turnOffRecommendedUiTimoutSettings(automan);
            PollingCheck.waitFor(() -> sameRecommendedTimeout(6000, 2000));
            turnOnRecommendedUiTimoutSettings(automan, 7000, 0);
            PollingCheck.waitFor(() -> sameRecommendedTimeout(7000, 2000));
            turnOnRecommendedUiTimoutSettings(automan, 0, 4000);
            PollingCheck.waitFor(() -> sameRecommendedTimeout(6000, 4000));
            turnOnRecommendedUiTimoutSettings(automan, 9000, 8000);
            PollingCheck.waitFor(() -> sameRecommendedTimeout(9000, 8000));
            turnOffRecommendedUiTimoutSettings(automan);
            PollingCheck.waitFor(() -> sameRecommendedTimeout(6000, 2000));
            assertEquals("Should return original timeout", 3000,
                    mAccessibilityManager.getRecommendedTimeoutMillis(3000,
                            AccessibilityManager.FLAG_CONTENT_ICONS));
            assertEquals("Should return original timeout", 7000,
                    mAccessibilityManager.getRecommendedTimeoutMillis(7000,
                            AccessibilityManager.FLAG_CONTENT_CONTROLS));
        } finally {
            automan.destroy();
        }
    }

    private void assertAtomicBooleanBecomes(AtomicBoolean atomicBoolean,
            boolean expectedValue, Object waitObject, String message)
            throws Exception {
        long timeoutTime =
                System.currentTimeMillis() + TIMEOUT_SERVICE_ENABLE;
        synchronized (waitObject) {
            while ((atomicBoolean.get() != expectedValue)
                    && (System.currentTimeMillis() < timeoutTime)) {
                waitObject.wait(timeoutTime - System.currentTimeMillis());
            }
        }
        assertTrue(message, atomicBoolean.get() == expectedValue);
    }

    private void waitForAccessibilityEnabled() throws InterruptedException {
        final Object waitObject = new Object();

        AccessibilityStateChangeListener listener = (boolean b) -> {
            synchronized (waitObject) {
                waitObject.notifyAll();
            }
        };
        mAccessibilityManager.addAccessibilityStateChangeListener(listener);
        long timeoutTime =
                System.currentTimeMillis() + TIMEOUT_SERVICE_ENABLE;
        synchronized (waitObject) {
            while (!mAccessibilityManager.isEnabled()
                    && (System.currentTimeMillis() < timeoutTime)) {
                waitObject.wait(timeoutTime - System.currentTimeMillis());
            }
        }
        mAccessibilityManager.removeAccessibilityStateChangeListener(listener);
        assertTrue("Timed out enabling accessibility", mAccessibilityManager.isEnabled());
    }

    private void turnOffRecommendedUiTimoutSettings(UiAutomation automan) {
        putSecureSetting(automan, ACCESSIBILITY_INTERACTIVE_UI_TIMEOUT_MS, null);
        putSecureSetting(automan, ACCESSIBILITY_NON_INTERACTIVE_UI_TIMEOUT_MS, null);
    }

    private void turnOnRecommendedUiTimoutSettings(UiAutomation automan,
            int interactiveUiTimeout, int nonInteractiveUiTimeout) {
        putSecureSetting(automan, ACCESSIBILITY_INTERACTIVE_UI_TIMEOUT_MS,
                Integer.toString(interactiveUiTimeout));
        putSecureSetting(automan, ACCESSIBILITY_NON_INTERACTIVE_UI_TIMEOUT_MS,
                Integer.toString(nonInteractiveUiTimeout));
    }

    private boolean sameRecommendedTimeout(int interactiveUiTimeout,
            int nonInteractiveUiTimeout) {
        final int currentInteractiveUiTimeout = mAccessibilityManager
                .getRecommendedTimeoutMillis(0, AccessibilityManager.FLAG_CONTENT_CONTROLS);
        final int currentNonInteractiveUiTimeout = mAccessibilityManager
                .getRecommendedTimeoutMillis(0, AccessibilityManager.FLAG_CONTENT_ICONS);
        return (currentInteractiveUiTimeout == interactiveUiTimeout
                && currentNonInteractiveUiTimeout == nonInteractiveUiTimeout);
    }

    private void putSecureSetting(UiAutomation automan, String name, String value) {
        final StringBuilder cmd = new StringBuilder("settings put secure ")
                .append(name).append(" ")
                .append(value);
        try {
            SystemUtil.runShellCommand(automan, cmd.toString());
        } catch (IOException e) {
            fail("Fail to run shell command");
        }
    }
}
