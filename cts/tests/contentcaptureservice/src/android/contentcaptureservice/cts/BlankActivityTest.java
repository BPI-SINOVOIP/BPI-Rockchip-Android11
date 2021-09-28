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
package android.contentcaptureservice.cts;

import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.contentcaptureservice.cts.CtsContentCaptureService.CONTENT_CAPTURE_SERVICE_COMPONENT_NAME;
import static android.contentcaptureservice.cts.Helper.resetService;
import static android.contentcaptureservice.cts.Helper.sContext;

import static com.android.compatibility.common.util.ActivitiesWatcher.ActivityLifecycle.DESTROYED;
import static com.android.compatibility.common.util.ActivitiesWatcher.ActivityLifecycle.RESUMED;

import static com.google.common.truth.Truth.assertThat;

import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Intent;
import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.platform.test.annotations.AppModeFull;
import android.support.test.uiautomator.UiDevice;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.ActivitiesWatcher.ActivityWatcher;

import org.junit.Before;
import org.junit.Test;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

@AppModeFull(reason = "BlankWithTitleActivityTest is enough")
public class BlankActivityTest
        extends AbstractContentCaptureIntegrationAutoActivityLaunchTest<BlankActivity> {

    private static final String TAG = BlankActivityTest.class.getSimpleName();

    private static final ActivityTestRule<BlankActivity> sActivityRule = new ActivityTestRule<>(
            BlankActivity.class, false, false);

    private UiDevice mDevice;

    public BlankActivityTest() {
        super(BlankActivity.class);
    }

    @Override
    protected ActivityTestRule<BlankActivity> getActivityTestRule() {
        return sActivityRule;
    }

    @Before
    public void setup() throws Exception {
        final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
        mDevice = UiDevice.getInstance(instrumentation);
    }

    @Test
    public void testSimpleSessionLifecycle() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final BlankActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        Log.v(TAG, "session id: " + session.id);

        activity.assertDefaultEvents(session);
    }

    @Test
    public void testGetServiceComponentName() throws Exception {
        final CtsContentCaptureService service = enableService();
        service.waitUntilConnected();

        final ActivityWatcher watcher = startWatcher();

        final BlankActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        try {
            assertThat(activity.getContentCaptureManager().getServiceComponentName())
                    .isEqualTo(CONTENT_CAPTURE_SERVICE_COMPONENT_NAME);

            resetService();
            service.waitUntilDisconnected();

            assertThat(activity.getContentCaptureManager().getServiceComponentName())
                    .isNotEqualTo(CONTENT_CAPTURE_SERVICE_COMPONENT_NAME);
        } finally {
            activity.finish();
            watcher.waitFor(DESTROYED);
        }
    }

    @Test
    public void testGetServiceComponentName_onUiThread() throws Exception {
        final CtsContentCaptureService service = enableService();
        service.waitUntilConnected();

        final ActivityWatcher watcher = startWatcher();

        final BlankActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        final AtomicReference<ComponentName> ref = new AtomicReference<>();
        activity.syncRunOnUiThread(
                () -> ref.set(activity.getContentCaptureManager().getServiceComponentName()));

        activity.finish();
        watcher.waitFor(DESTROYED);

        assertThat(ref.get()).isEqualTo(CONTENT_CAPTURE_SERVICE_COMPONENT_NAME);
    }

    @Test
    public void testIsContentCaptureFeatureEnabled_onUiThread() throws Exception {
        final CtsContentCaptureService service = enableService();
        service.waitUntilConnected();

        final ActivityWatcher watcher = startWatcher();

        final BlankActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        final AtomicBoolean ref = new AtomicBoolean();
        activity.syncRunOnUiThread(() -> ref
                .set(activity.getContentCaptureManager().isContentCaptureFeatureEnabled()));

        activity.finish();
        watcher.waitFor(DESTROYED);

        assertThat(ref.get()).isTrue();
    }

    @Test
    public void testDisableContentCaptureService_onUiThread() throws Exception {
        final CtsContentCaptureService service = enableService();
        service.waitUntilConnected();

        final ActivityWatcher watcher = startWatcher();

        final BlankActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        service.disableSelf();

        activity.finish();
        watcher.waitFor(DESTROYED);
    }

    @Test
    public void testOnConnectionEvents() throws Exception {
        final CtsContentCaptureService service = enableService();
        service.waitUntilConnected();

        resetService();
        service.waitUntilDisconnected();
    }

    @Test
    public void testOutsideOfPackageActivity_noSessionCreated() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        Intent outsideActivity = new Intent();
        outsideActivity.setComponent(new ComponentName("android.contentcaptureservice.cts2",
                "android.contentcaptureservice.cts2.OutsideOfPackageActivity"));
        outsideActivity.setFlags(FLAG_ACTIVITY_NEW_TASK);

        sContext.startActivity(outsideActivity);

        mDevice.waitForIdle();

        assertThat(service.getAllSessionIds()).isEmpty();
    }
}
