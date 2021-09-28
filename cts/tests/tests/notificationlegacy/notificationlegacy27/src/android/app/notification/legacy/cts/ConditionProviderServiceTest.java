/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.app.notification.legacy.cts;

import static android.app.NotificationManager.INTERRUPTION_FILTER_ALARMS;
import static android.app.NotificationManager.INTERRUPTION_FILTER_NONE;
import static android.service.notification.NotificationListenerService.INTERRUPTION_FILTER_PRIORITY;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNull;
import static junit.framework.TestCase.fail;

import static java.lang.Thread.sleep;

import android.app.AutomaticZenRule;
import android.app.Instrumentation;
import android.app.NotificationManager;
import android.app.UiAutomation;
import android.content.ComponentName;
import android.content.Context;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.service.notification.Condition;
import android.service.notification.ZenModeConfig;
import android.util.ArraySet;
import android.util.Log;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import junit.framework.Assert;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;

@RunWith(AndroidJUnit4.class)
public class ConditionProviderServiceTest {
    private static String TAG = "CpsTest";

    private NotificationManager mNm;
    private Context mContext;
    private ZenModeBroadcastReceiver mModeReceiver;
    private IntentFilter mModeFilter;
    private ArraySet<String> ids = new ArraySet<>();

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getContext();
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), true);
        LegacyConditionProviderService.requestRebind(LegacyConditionProviderService.getId());
        SecondaryConditionProviderService.requestRebind(SecondaryConditionProviderService.getId());
        mNm = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        mNm.setInterruptionFilter(NotificationManager.INTERRUPTION_FILTER_ALL);
        mModeReceiver = new ZenModeBroadcastReceiver();
        mModeFilter = new IntentFilter();
        mModeFilter.addAction(NotificationManager.ACTION_INTERRUPTION_FILTER_CHANGED);
        mContext.registerReceiver(mModeReceiver, mModeFilter);
    }

    @After
    public void tearDown() throws Exception {
        mContext.unregisterReceiver(mModeReceiver);
        if (mNm == null) {
            // assumption in setUp is false, so mNm is not initialized
            return;
        }
        try {
            for (String id : ids) {
                if (id != null) {
                    if (!mNm.removeAutomaticZenRule(id)) {
                        throw new Exception("Could not remove rule " + id);
                    }
                    sleep(100);
                    assertNull(mNm.getAutomaticZenRule(id));
                }
            }
        } finally {
            toggleNotificationPolicyAccess(mContext.getPackageName(),
                    InstrumentationRegistry.getInstrumentation(), false);
            pollForConnection(LegacyConditionProviderService.class, false);
            pollForConnection(SecondaryConditionProviderService.class, false);
        }
    }

    @Test
    public void testUnboundCPSMaintainsCondition_addsNewRule() throws Exception {
        // make sure service get bound
        pollForConnection(SecondaryConditionProviderService.class, true);

        final ComponentName cn = SecondaryConditionProviderService.getId();

        // add rule
        mModeReceiver.reset();

        addRule(cn, INTERRUPTION_FILTER_ALARMS, true);
        pollForSubscribe(SecondaryConditionProviderService.getInstance());

        mModeReceiver.waitFor(1/*Secondary only*/, 1000/*Limit is 1 second*/);
        assertEquals(INTERRUPTION_FILTER_ALARMS, mNm.getCurrentInterruptionFilter());

        // unbind service
        SecondaryConditionProviderService.getInstance().requestUnbind();

        // verify that DND state doesn't change
        assertEquals(INTERRUPTION_FILTER_ALARMS, mNm.getCurrentInterruptionFilter());

        // add a new rule
        addRule(cn, INTERRUPTION_FILTER_NONE, true);

        // verify that the unbound service maintains it's DND vote
        assertEquals(INTERRUPTION_FILTER_ALARMS, mNm.getCurrentInterruptionFilter());
    }

    @Test
    public void testUnboundCPSMaintainsCondition_otherConditionChanges() throws Exception {
        // make sure both services get bound
        pollForConnection(LegacyConditionProviderService.class, true);
        pollForConnection(SecondaryConditionProviderService.class, true);

        // add rules for both
        mModeReceiver.reset();

        addRule(LegacyConditionProviderService.getId(), INTERRUPTION_FILTER_PRIORITY, true);
        pollForSubscribe(LegacyConditionProviderService.getInstance());

        addRule(SecondaryConditionProviderService.getId(), INTERRUPTION_FILTER_ALARMS, true);
        pollForSubscribe(SecondaryConditionProviderService.getInstance());

        mModeReceiver.waitFor(2/*Legacy and Secondary*/, 1000/*Limit is 1 second*/);
        assertEquals(INTERRUPTION_FILTER_ALARMS, mNm.getCurrentInterruptionFilter());

        // unbind one of the services
        SecondaryConditionProviderService.getInstance().requestUnbind();

        // verify that DND state doesn't change
        assertEquals(INTERRUPTION_FILTER_ALARMS, mNm.getCurrentInterruptionFilter());

        // trigger a change in the bound service's condition
        ((LegacyConditionProviderService) LegacyConditionProviderService.getInstance())
                .toggleDND(false);
        sleep(500);

        // verify that the unbound service maintains it's DND vote
        assertEquals(INTERRUPTION_FILTER_ALARMS, mNm.getCurrentInterruptionFilter());
    }

    @Test
    public void testUnboundCPSMaintainsCondition_otherProviderRuleChanges() throws Exception {
        // make sure both services get bound
        pollForConnection(LegacyConditionProviderService.class, true);
        pollForConnection(SecondaryConditionProviderService.class, true);

        // add rules for both
        mModeReceiver.reset();

        addRule(LegacyConditionProviderService.getId(), INTERRUPTION_FILTER_PRIORITY, true);
        pollForSubscribe(LegacyConditionProviderService.getInstance());

        addRule(SecondaryConditionProviderService.getId(), INTERRUPTION_FILTER_ALARMS, true);
        pollForSubscribe(SecondaryConditionProviderService.getInstance());

        mModeReceiver.waitFor(2/*Legacy and Secondary*/, 1000/*Limit is 1 second*/);
        assertEquals(INTERRUPTION_FILTER_ALARMS, mNm.getCurrentInterruptionFilter());

        // unbind one of the services
        SecondaryConditionProviderService.getInstance().requestUnbind();

        // verify that DND state doesn't change
        assertEquals(INTERRUPTION_FILTER_ALARMS, mNm.getCurrentInterruptionFilter());

        // trigger a change in the bound service's rule
        addRule(SecondaryConditionProviderService.getId(), INTERRUPTION_FILTER_PRIORITY, false);

        // verify that the unbound service maintains it's DND vote
        assertEquals(INTERRUPTION_FILTER_ALARMS, mNm.getCurrentInterruptionFilter());
    }

    @Test
    public void testRequestRebindWhenLostAccess() throws Exception {
        // make sure it gets bound
        pollForConnection(LegacyConditionProviderService.class, true);

        // request unbind
        LegacyConditionProviderService.getInstance().requestUnbind();

        // make sure it unbinds
        pollForConnection(LegacyConditionProviderService.class, false);

        // lose dnd access
        toggleNotificationPolicyAccess(mContext.getPackageName(),
                InstrumentationRegistry.getInstrumentation(), false);

        // try to rebind
        LegacyConditionProviderService.requestRebind(LegacyConditionProviderService.getId());

        // make sure it isn't rebound
        try {
            pollForConnection(LegacyConditionProviderService.class, true);
            fail("Service got rebound after permission lost");
        } catch (Exception e) {
            // pass
        }
    }

    @Test
    public void testRequestRebindWhenStillHasAccess() throws Exception {
        // make sure it gets bound
        pollForConnection(LegacyConditionProviderService.class, true);

        // request unbind
        LegacyConditionProviderService.getInstance().requestUnbind();

        // make sure it unbinds
        pollForConnection(LegacyConditionProviderService.class, false);

        // try to rebind
        LegacyConditionProviderService.requestRebind(LegacyConditionProviderService.getId());

        // make sure it did rebind
        try {
            pollForConnection(LegacyConditionProviderService.class, true);
        } catch (Exception e) {
            fail("Service should've been able to rebind");
        }
    }

    @Test
    public void testMethodsExistAndDoNotThrow() throws Exception {
        // make sure it gets bound
        pollForConnection(LegacyConditionProviderService.class, true);

        // request unbind
        LegacyConditionProviderService.getInstance().onConnected();
        LegacyConditionProviderService.getInstance().onRequestConditions(
                Condition.FLAG_RELEVANT_NOW);
        LegacyConditionProviderService.getInstance().onSubscribe(Uri.EMPTY);
        LegacyConditionProviderService.getInstance().onUnsubscribe(Uri.EMPTY);

    }

    private void addRule(ComponentName cn, int filter, boolean enabled) {
        final Uri conditionId = new Uri.Builder()
                .scheme(Condition.SCHEME)
                .authority(ZenModeConfig.SYSTEM_AUTHORITY)
                .appendPath(cn.toString())
                .build();
        String id = mNm.addAutomaticZenRule(new AutomaticZenRule("name",
                cn, conditionId, filter, enabled));
        Log.d(TAG, "Created rule with id " + id);
        ids.add(id);
    }

    private void toggleNotificationPolicyAccess(String packageName,
            Instrumentation instrumentation, boolean on) throws IOException {

        String command = " cmd notification " + (on ? "allow_dnd " : "disallow_dnd ") + packageName;

        runCommand(command, instrumentation);

        NotificationManager nm = mContext.getSystemService(NotificationManager.class);
        Assert.assertEquals("Notification Policy Access Grant is " +
                        nm.isNotificationPolicyAccessGranted() + " not " + on, on,
                nm.isNotificationPolicyAccessGranted());
    }

    private void runCommand(String command, Instrumentation instrumentation) throws IOException {
        UiAutomation uiAutomation = instrumentation.getUiAutomation();
        // Execute command
        try (ParcelFileDescriptor fd = uiAutomation.executeShellCommand(command)) {
            Assert.assertNotNull("Failed to execute shell command: " + command, fd);
            // Wait for the command to finish by reading until EOF
            try (InputStream in = new FileInputStream(fd.getFileDescriptor())) {
                byte[] buffer = new byte[4096];
                while (in.read(buffer) > 0) {}
            } catch (IOException e) {
                throw new IOException("Could not read stdout of command: " + command, e);
            }
        } finally {
            uiAutomation.destroy();
        }
    }

    private void pollForSubscribe(PollableConditionProviderService service)  throws Exception {
        int tries = 30;
        int delayMs = 200;

        while (tries-- > 0 && !service.subscribed) {
            try {
                sleep(delayMs);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }

        if (!service.subscribed) {
            Log.d(TAG, "not subscribed");
            throw new Exception("Service never got onSubscribe()");
        }
    }

    private void pollForConnection(Class<? extends PollableConditionProviderService> service,
            boolean waitForConnection) throws Exception {
        int tries = 100;
        int delayMs = 200;

        PollableConditionProviderService instance =
                (PollableConditionProviderService) service.getMethod("getInstance").invoke(null);

        while (tries-- > 0 && (waitForConnection ? instance == null : instance != null)) {
            try {
                sleep(delayMs);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            instance = (PollableConditionProviderService) service.getMethod("getInstance")
                    .invoke(null);
        }

        if (waitForConnection && instance == null) {
            Log.d(TAG, service.getName() + " not bound");
            throw new Exception("CPS never bound");
        } else if (!waitForConnection && instance != null) {
            Log.d(TAG, service.getName() + " still bound");
            throw new Exception("CPS still bound");
        } else {
            Log.d(TAG, service.getName() + " has a correct bind state");
        }
    }
}
