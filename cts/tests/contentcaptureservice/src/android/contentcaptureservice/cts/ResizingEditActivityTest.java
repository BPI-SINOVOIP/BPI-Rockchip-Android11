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
package android.contentcaptureservice.cts;

import static android.contentcaptureservice.cts.Assertions.assertRightActivity;
import static android.contentcaptureservice.cts.Assertions.assertViewInsetsChanged;

import static com.android.compatibility.common.util.ActivitiesWatcher.ActivityLifecycle.DESTROYED;
import static com.android.compatibility.common.util.ActivitiesWatcher.ActivityLifecycle.RESUMED;

import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.platform.test.annotations.AppModeFull;
import android.view.contentcapture.ContentCaptureEvent;
import android.view.contentcapture.ContentCaptureSessionId;

import androidx.annotation.NonNull;
import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.ActivitiesWatcher.ActivityWatcher;
import com.android.compatibility.common.util.RetryRule;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;

import java.util.List;

@AppModeFull(reason = "BlankWithTitleActivityTest is enough")
public class ResizingEditActivityTest
        extends AbstractContentCaptureIntegrationAutoActivityLaunchTest<ResizingEditActivity> {
    private static final int INITIAL_EVENT_WAIT_TIMEOUT_MS = 500;
    private static int sEventWaitTimeoutMs = INITIAL_EVENT_WAIT_TIMEOUT_MS;
    private static final int NUM_RETRIES = 3;

    private static final ActivityTestRule<ResizingEditActivity> sActivityRule =
            new ActivityTestRule<>(ResizingEditActivity.class, false, false);
    private static final RetryRule sRetryRule = new RetryRule(NUM_RETRIES, () -> {
        sEventWaitTimeoutMs *= 2;
    });
    private static final RuleChain sRuleChain = RuleChain
            .outerRule(sRetryRule)
            .around(sActivityRule);


    public ResizingEditActivityTest() {
        super(ResizingEditActivity.class);
    }

    @Override
    protected ActivityTestRule<ResizingEditActivity> getActivityTestRule() {
        return sActivityRule;
    }

    @NonNull
    @Override
    protected TestRule getMainTestRule() {
        return sRuleChain;
    }

    @Before
    @After
    public void resetActivityStaticState() {
        ResizingEditActivity.onRootView(null);
    }

    @Test
    public void testInsetsChangedOnImeAction() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivityWatcher watcher = startWatcher();

        final ResizingEditActivity activity = launchActivity();

        watcher.waitFor(RESUMED);

        activity.syncRunOnUiThread(() -> {
            activity.showSoftInputOnExtraInputEditText();
        });

        // Showing the IME is an asynchronous operation which we cannot wait for,
        // so sleep a little while in order to allow time for the keyboard event
        // to propagate.
        try {
            Thread.currentThread().sleep(sEventWaitTimeoutMs);
        } catch (Exception ex) { }

        activity.finish();
        watcher.waitFor(DESTROYED);

        final Session session = service.getOnlyFinishedSession();
        final ContentCaptureSessionId sessionId = session.id;

        assertRightActivity(session, sessionId, activity);

        final List<ContentCaptureEvent> events = session.getEvents();

        assertViewInsetsChanged(events);
    }
}
