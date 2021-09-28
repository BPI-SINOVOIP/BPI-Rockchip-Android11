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
package android.contentcaptureservice.cts;

import static com.android.compatibility.common.util.ActivitiesWatcher.ActivityLifecycle.DESTROYED;
import static com.android.compatibility.common.util.ActivitiesWatcher.ActivityLifecycle.RESUMED;

import android.content.Intent;
import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;

import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.ActivitiesWatcher.ActivityWatcher;

import org.junit.Test;

public class BlankWithTitleActivityTest
        extends AbstractContentCaptureIntegrationAutoActivityLaunchTest<BlankWithTitleActivity> {

    private static final String TAG = BlankWithTitleActivityTest.class.getSimpleName();

    private static final ActivityTestRule<BlankWithTitleActivity> sActivityRule =
            new ActivityTestRule<>(BlankWithTitleActivity.class, false, false);

    public BlankWithTitleActivityTest() {
        super(BlankWithTitleActivity.class);
    }

    @Override
    protected ActivityTestRule<BlankWithTitleActivity> getActivityTestRule() {
        return sActivityRule;
    }

    @Test
    public void testSimpleSessionLifecycle() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final BlankWithTitleActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        Log.v(TAG, "session id: " + session.id);

        activity.assertDefaultEvents(session);
    }

    @AppModeFull(reason = "testSimpleSessionLifecycle() is enough")
    @Test
    public void testSimpleSessionLifecycle_noAnimation() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final BlankWithTitleActivity activity = launchActivity(
                (intent) -> intent.addFlags(
                        Intent.FLAG_ACTIVITY_NO_ANIMATION | Intent.FLAG_ACTIVITY_NEW_TASK));
        watcher.waitFor(RESUMED);

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        Log.v(TAG, "session id: " + session.id);

        activity.assertDefaultEvents(session);
    }
}
