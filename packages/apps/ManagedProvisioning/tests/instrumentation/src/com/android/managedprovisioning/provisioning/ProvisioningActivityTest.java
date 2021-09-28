/*
 * Copyright 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.provisioning;

import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_FINANCED_DEVICE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_DEVICE;
import static android.app.admin.DevicePolicyManager.ACTION_PROVISION_MANAGED_PROFILE;
import static android.app.admin.DevicePolicyManager.ACTION_STATE_USER_SETUP_COMPLETE;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasComponent;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.core.AllOf.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mockingDetails;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest.permission;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Color;
import android.os.Bundle;
import android.provider.Settings;

import androidx.test.InstrumentationRegistry;
import androidx.test.espresso.intent.rule.IntentsTestRule;
import androidx.test.filters.FlakyTest;
import androidx.test.filters.SmallTest;

import com.android.managedprovisioning.R;
import com.android.managedprovisioning.TestInstrumentationRunner;
import com.android.managedprovisioning.common.LogoUtils;
import com.android.managedprovisioning.common.PolicyComplianceUtils;
import com.android.managedprovisioning.common.Utils;
import com.android.managedprovisioning.finalization.UserProvisioningStateHelper;
import com.android.managedprovisioning.model.ProvisioningParams;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.hamcrest.MockitoHamcrest;
import org.mockito.invocation.Invocation;
import org.mockito.junit.MockitoJUnitRunner;

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Unit tests for {@link ProvisioningActivity}.
 */
@SmallTest
@RunWith(MockitoJUnitRunner.class)
public class ProvisioningActivityTest {

    private static final String ADMIN_PACKAGE = "com.test.admin";
    private static final String TEST_PACKAGE = "com.android.managedprovisioning.tests";
    private static final ComponentName ADMIN = new ComponentName(ADMIN_PACKAGE, ".Receiver");
    private static final ComponentName TEST_ACTIVITY = new ComponentName(TEST_PACKAGE,
            EmptyActivity.class.getCanonicalName());
    public static final ProvisioningParams PROFILE_OWNER_PARAMS = new ProvisioningParams.Builder()
            .setProvisioningAction(ACTION_PROVISION_MANAGED_PROFILE)
            .setDeviceAdminComponentName(ADMIN)
            .build();
    public static final ProvisioningParams DEVICE_OWNER_PARAMS = new ProvisioningParams.Builder()
            .setProvisioningAction(ACTION_PROVISION_MANAGED_DEVICE)
            .setDeviceAdminComponentName(ADMIN)
            .build();
    private static final ProvisioningParams FINANCED_DEVICE_PARAMS = new ProvisioningParams
            .Builder()
            .setProvisioningAction(ACTION_PROVISION_FINANCED_DEVICE)
            .setDeviceAdminComponentName(ADMIN)
            .build();
    private static final ProvisioningParams NFC_PARAMS = new ProvisioningParams.Builder()
            .setProvisioningAction(ACTION_PROVISION_MANAGED_PROFILE)
            .setDeviceAdminComponentName(ADMIN)
            .setStartedByTrustedSource(true)
            .setIsNfc(true)
            .build();
    private static final Intent PROFILE_OWNER_INTENT = new Intent()
            .putExtra(ProvisioningParams.EXTRA_PROVISIONING_PARAMS, PROFILE_OWNER_PARAMS);
    private static final Intent DEVICE_OWNER_INTENT = new Intent()
            .putExtra(ProvisioningParams.EXTRA_PROVISIONING_PARAMS, DEVICE_OWNER_PARAMS);
    private static final Intent FINANCED_DEVICE_INTENT = new Intent()
            .putExtra(ProvisioningParams.EXTRA_PROVISIONING_PARAMS, FINANCED_DEVICE_PARAMS);
    private static final Intent NFC_INTENT = new Intent()
            .putExtra(ProvisioningParams.EXTRA_PROVISIONING_PARAMS, NFC_PARAMS);
    private static final int DEFAULT_MAIN_COLOR = Color.rgb(1, 2, 3);
    private static final int BROADCAST_TIMEOUT = 1000;
    private static final int WAIT_PROVISIONING_COMPLETE_MILLIS = 60_000;

    private static class CustomIntentsTestRule extends IntentsTestRule<ProvisioningActivity> {
        private boolean mIsActivityRunning = false;
        private CustomIntentsTestRule() {
            super(ProvisioningActivity.class, true /* Initial touch mode  */,
                    false /* Lazily launch activity */);
        }

        @Override
        protected synchronized void afterActivityLaunched() {
            mIsActivityRunning = true;
            super.afterActivityLaunched();
        }

        @Override
        public synchronized void afterActivityFinished() {
            // Temp fix for b/37663530
            if (mIsActivityRunning) {
                super.afterActivityFinished();
                mIsActivityRunning = false;
            }
        }
    }

    @Rule
    public CustomIntentsTestRule mActivityRule = new CustomIntentsTestRule();

    @Mock private ProvisioningManager mProvisioningManager;
    @Mock private PackageManager mPackageManager;
    @Mock private UserProvisioningStateHelper mUserProvisioningStateHelper;
    private Utils mUtils;
    private static int mRotationLocked;

    @BeforeClass
    public static void setUpClass() {
        // Stop the activity from rotating in order to keep hold of the context
        Context context = InstrumentationRegistry.getTargetContext();

        mRotationLocked = Settings.System.getInt(context.getContentResolver(),
                Settings.System.ACCELEROMETER_ROTATION, 0);
        Settings.System.putInt(context.getContentResolver(),
                Settings.System.ACCELEROMETER_ROTATION, 0);
    }

    @Before
    public void setup() {
        mUtils = spy(new Utils());
        doNothing().when(mUtils).sendFactoryResetBroadcast(any(Context.class), anyString());
        doReturn(DEFAULT_MAIN_COLOR).when(mUtils).getAccentColor(any());
    }

    @AfterClass
    public static void tearDownClass() {
        // Reset the rotation value back to what it was before the test
        Context context = InstrumentationRegistry.getTargetContext();

        Settings.System.putInt(context.getContentResolver(),
                Settings.System.ACCELEROMETER_ROTATION, mRotationLocked);
    }

    @Before
    public void setUp() {
        TestInstrumentationRunner.registerReplacedActivity(ProvisioningActivity.class,
                (classLoader, className, intent) ->
                        new ProvisioningActivity(
                                mProvisioningManager, mUtils, mUserProvisioningStateHelper,
                                new PolicyComplianceUtils()) {
                            @Override
                            public PackageManager getPackageManager() {
                                return mPackageManager;
                            }
                        });
        // LogoUtils cached icon globally. Clean-up the cache
        LogoUtils.cleanUp(InstrumentationRegistry.getTargetContext());
    }

    @After
    public void tearDown() {
        TestInstrumentationRunner.unregisterReplacedActivity(ProvisioningActivity.class);
    }

    @Test
    public void testLaunch() throws NoSuchMethodException {
        // GIVEN the activity was launched with a profile owner intent
        launchActivityAndWait(PROFILE_OWNER_INTENT);
        // THEN the provisioning process should be initiated
        verify(mProvisioningManager).maybeStartProvisioning(PROFILE_OWNER_PARAMS);

        // THEN the activity should start listening for provisioning updates
        final Method registerListenerMethod = ProvisioningManager.class
                .getMethod("registerListener", ProvisioningManagerCallback.class);
        final int registerListenerInvocations = getNumberOfInvocations(registerListenerMethod);
        final Method unregisterListenerMethod = ProvisioningManager.class
            .getMethod("unregisterListener", ProvisioningManagerCallback.class);
        final int unregisterListenerInvocations = getNumberOfInvocations(unregisterListenerMethod);
        assertThat(registerListenerInvocations - unregisterListenerInvocations).isEqualTo(1);
    }

    private int getNumberOfInvocations(Method method) {
        final Collection<Invocation> invocations =
                mockingDetails(mProvisioningManager).getInvocations();
        return (int) invocations.stream()
                .filter(invocation -> invocation.getMethod().equals(method)).count();
    }

    @Test
    public void testSavedInstanceState() throws Throwable {
        // GIVEN the activity was launched with a profile owner intent
        launchActivityAndWait(PROFILE_OWNER_INTENT);

        // THEN the provisioning process should be initiated
        verify(mProvisioningManager).maybeStartProvisioning(PROFILE_OWNER_PARAMS);

        // WHEN the activity is recreated with a saved instance state
        mActivityRule.runOnUiThread(() -> {
            Bundle bundle = new Bundle();
            InstrumentationRegistry.getInstrumentation()
                    .callActivityOnSaveInstanceState(mActivityRule.getActivity(), bundle);
            InstrumentationRegistry.getInstrumentation()
                    .callActivityOnCreate(mActivityRule.getActivity(), bundle);
        });

        // THEN provisioning should not be initiated again
        verify(mProvisioningManager).maybeStartProvisioning(PROFILE_OWNER_PARAMS);
    }

    @Test
    public void testPause() throws Throwable {
        // GIVEN the activity was launched with a profile owner intent
        launchActivityAndWait(PROFILE_OWNER_INTENT);

        reset(mProvisioningManager);

        // WHEN the activity is paused
        mActivityRule.runOnUiThread(() -> {
            InstrumentationRegistry.getInstrumentation()
                    .callActivityOnPause(mActivityRule.getActivity());
        });

        // THEN the listener is unregistered
        // b/130350469 to figure out why onPause/onResume is called one additional time
        verify(mProvisioningManager).unregisterListener(any(ProvisioningManagerCallback.class));
    }

    @Test
    public void testCancelProfileOwner_CompProvisioningWithSkipConsent() throws Throwable {
        // GIVEN launching profile intent with skipping user consent
        ProvisioningParams params = new ProvisioningParams.Builder()
                .setProvisioningAction(ACTION_PROVISION_MANAGED_PROFILE)
                .setDeviceAdminComponentName(ADMIN)
                .setSkipUserConsent(true)
                .build();
        Intent intent = new Intent()
                .putExtra(ProvisioningParams.EXTRA_PROVISIONING_PARAMS, params);
        launchActivityAndWait(new Intent(intent));

        reset(mProvisioningManager);

        // WHEN the user tries to cancel
        mActivityRule.runOnUiThread(() -> mActivityRule.getActivity().onBackPressed());

        // THEN never unregistering ProvisioningManager
        // b/130350469 to figure out why onPause/onResume is called one additional time
        verify(mProvisioningManager, never()).unregisterListener(
                any(ProvisioningManagerCallback.class));
    }

    @FlakyTest
    @Test
    public void testCancelProfileOwner_CompProvisioningWithoutSkipConsent() throws Throwable {
        // GIVEN launching profile intent without skipping user consent
        ProvisioningParams params = new ProvisioningParams.Builder()
                .setProvisioningAction(ACTION_PROVISION_MANAGED_PROFILE)
                .setDeviceAdminComponentName(ADMIN)
                .setSkipUserConsent(false)
                .build();
        Intent intent = new Intent()
                .putExtra(ProvisioningParams.EXTRA_PROVISIONING_PARAMS, params);
        launchActivityAndWait(new Intent(intent));

        reset(mProvisioningManager);

        // WHEN the user tries to cancel
        mActivityRule.runOnUiThread(() -> mActivityRule.getActivity().onBackPressed());

        // THEN unregistering ProvisioningManager
        // b/130350469 to figure out why onPause/onResume is called one additional time
        verify(mProvisioningManager)
                .unregisterListener(any(ProvisioningManagerCallback.class));

        // THEN the cancel dialog should be shown
        onView(withText(R.string.profile_owner_cancel_message)).check(matches(isDisplayed()));
    }

    @Test
    public void testCancelDeviceOwner() throws Throwable {
        // GIVEN the activity was launched with a device owner intent
        launchActivityAndWait(DEVICE_OWNER_INTENT);

        // WHEN the user tries to cancel
        pressBack();

        // THEN the cancel dialog should be shown
        onView(withText(R.string.stop_setup_reset_device_question)).check(matches(isDisplayed()));
        onView(withText(R.string.this_will_reset_take_back_first_screen))
                .check(matches(isDisplayed()));

        // WHEN deciding not to cancel
        onView(withId(android.R.id.button2))
                .check(matches(withText(R.string.device_owner_cancel_cancel)))
                .perform(click());

        // THEN the activity should not be finished
        assertFalse(mActivityRule.getActivity().isFinishing());

        // WHEN the user tries to cancel
        pressBack();

        // THEN the cancel dialog should be shown
        onView(withText(R.string.stop_setup_reset_device_question)).check(matches(isDisplayed()));

        // WHEN deciding to cancel
        onView(withId(android.R.id.button1))
                .check(matches(withText(R.string.reset)))
                .perform(click());

        // THEN factory reset should be invoked
        verify(mUtils, timeout(BROADCAST_TIMEOUT))
                .sendFactoryResetBroadcast(any(Context.class), anyString());
    }

    @Test
    public void testSuccess() throws Throwable {
        // GIVEN the activity was launched with a profile owner intent
        launchActivityAndWait(PROFILE_OWNER_INTENT);

        // WHEN preFinalization is completed
        mActivityRule.runOnUiThread(() -> mActivityRule.getActivity().preFinalizationCompleted());

        Thread.sleep(WAIT_PROVISIONING_COMPLETE_MILLIS);

        // Press next button on provisioning complete
        onView(withText(R.string.next)).perform(click());

        // THEN the activity should finish
        onView(withId(R.id.provisioning_progress));
        assertTrue(mActivityRule.getActivity().isFinishing());
    }

    @Test
    public void testSuccess_Nfc() throws Throwable {
        // GIVEN queryIntentActivities return test_activity
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = TEST_ACTIVITY.getPackageName();
        activityInfo.name = TEST_ACTIVITY.getClassName();
        activityInfo.permission = permission.BIND_DEVICE_ADMIN;
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        List<ResolveInfo> resolveInfoList = new ArrayList();
        resolveInfoList.add(resolveInfo);
        when(mPackageManager.queryIntentActivities(
                MockitoHamcrest.argThat(hasAction(ACTION_STATE_USER_SETUP_COMPLETE)),
                eq(0))).thenReturn(resolveInfoList);
        when(mPackageManager.checkPermission(eq(permission.DISPATCH_PROVISIONING_MESSAGE),
                eq(activityInfo.packageName))).thenReturn(PackageManager.PERMISSION_GRANTED);

        // GIVEN the activity was launched with a nfc intent
        launchActivityAndWait(NFC_INTENT);

        // WHEN preFinalization is completed
        mActivityRule.runOnUiThread(() -> mActivityRule.getActivity().preFinalizationCompleted());

        Thread.sleep(WAIT_PROVISIONING_COMPLETE_MILLIS);

        // Press next button on provisioning complete
        onView(withText(R.string.next)).perform(click());

        // THEN verify starting TEST_ACTIVITY
        intended(allOf(hasComponent(TEST_ACTIVITY), hasAction(ACTION_STATE_USER_SETUP_COMPLETE)));

        // THEN the activity should finish
        onView(withId(R.id.provisioning_progress));
        assertTrue(mActivityRule.getActivity().isFinishing());
    }

    @Test
    public void testInitializeUi_profileOwner() throws Throwable {
        // GIVEN the activity was launched with a profile owner intent
        launchActivityAndWait(PROFILE_OWNER_INTENT);

        // THEN the profile owner description should be present
        onView(withId(R.id.provisioning_progress))
                .check(matches(withText(R.string.work_profile_provisioning_progress_label)));

        // THEN the animation is shown.
        onView(withId(R.id.animation)).check(matches(isDisplayed()));
    }

    @Test
    public void testInitializeUi_deviceOwner() throws Throwable {
        // GIVEN the activity was launched with a device owner intent
        launchActivityAndWait(DEVICE_OWNER_INTENT);

        // THEN the description should be empty
        onView(withId(R.id.provisioning_progress)).check(
                matches(withText(R.string.fully_managed_device_provisioning_progress_label)));

        // THEN the animation is shown.
        onView(withId(R.id.animation)).check(matches(isDisplayed()));
    }

    @Test
    public void testInitializeUi_financedDevice() throws Throwable {
        // GIVEN the activity was launched with a financed device intent
        launchActivityAndWait(FINANCED_DEVICE_INTENT);

        // THEN the header will be set
        onView(withId(R.id.suc_layout_title)).check(matches(withText(R.string.just_a_sec)));

        // THEN the icon will be invisible
        onView(withId(R.id.sud_layout_icon)).check(matches(not(isDisplayed())));

        // THEN the animation is shown.
        onView(withId(R.id.animation)).check(matches(isDisplayed()));
    }

    private void launchActivityAndWait(Intent intent) {
        mActivityRule.launchActivity(intent);
        onView(withId(R.id.setup_wizard_layout));
    }
}
