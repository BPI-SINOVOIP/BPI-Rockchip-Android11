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

import static com.android.compatibility.common.util.ActivitiesWatcher.ActivityLifecycle.RESUMED;

import static com.google.common.truth.Truth.assertThat;

import android.platform.test.annotations.AppModeFull;

import androidx.test.rule.ActivityTestRule;

import com.android.compatibility.common.util.ActivitiesWatcher;
import com.android.compatibility.common.util.PollingCheck;

import org.junit.Test;

import java.util.Arrays;

@AppModeFull(reason = "BlankWithTitleActivityTest is enough")
public class DataSharingActivityTest
        extends AbstractContentCaptureIntegrationAutoActivityLaunchTest<DataSharingActivity>  {

    private static final String TAG = DataSharingActivityTest.class.getSimpleName();

    private static final ActivityTestRule<DataSharingActivity> sActivityRule =
            new ActivityTestRule<>(DataSharingActivity.class, false, false);

    public DataSharingActivityTest() {
        super(DataSharingActivity.class);
    }

    @Override
    protected ActivityTestRule<DataSharingActivity> getActivityTestRule() {
        return sActivityRule;
    }

    @Test
    public void testHappyPath_dataCopiedSuccessfully() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivitiesWatcher.ActivityWatcher watcher = startWatcher();

        service.setDataSharingEnabled(true);

        DataSharingActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        PollingCheck.waitFor(() -> activity.mSessionFinished);
        PollingCheck.waitFor(() -> service.mDataShareSessionFinished);

        assertThat(activity.mSessionSucceeded).isTrue();
        assertThat(service.mDataShareSessionSucceeded).isTrue();

        assertThat(activity.mDataWritten).isEqualTo(
                Arrays.copyOfRange(service.mDataShared, 0, activity.mDataWritten.length));
    }

    @Test
    public void testDataSharingSessionIsRejected_propagatedToClient() throws Exception {
        final CtsContentCaptureService service = enableService();
        final ActivitiesWatcher.ActivityWatcher watcher = startWatcher();

        service.setDataSharingEnabled(false);

        DataSharingActivity activity = launchActivity();
        watcher.waitFor(RESUMED);

        PollingCheck.waitFor(() -> activity.mSessionFinished);
        PollingCheck.waitFor(() -> service.mDataShareSessionFinished);

        assertThat(activity.mSessionSucceeded).isFalse();
        assertThat(service.mDataShareSessionSucceeded).isFalse();

        assertThat(activity.mSessionRejected).isTrue();
    }
}
