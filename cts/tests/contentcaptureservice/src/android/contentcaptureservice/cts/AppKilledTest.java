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

import static android.contentcaptureservice.cts.Assertions.assertNoEvents;
import static android.contentcaptureservice.cts.OutOfProcessActivity.killOutOfProcessActivity;
import static android.contentcaptureservice.cts.OutOfProcessActivity.startAndWaitOutOfProcessActivity;

import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;

import org.junit.Test;

@AppModeFull(reason = "BlankWithTitleActivityTest is enough")
public class AppKilledTest extends AbstractContentCaptureIntegrationActivityLessTest {

    @Test
    public void testDoIt() throws Exception {
        final CtsContentCaptureService service = enableService();

        startAndWaitOutOfProcessActivity();

        killOutOfProcessActivity();

        final Session session = service.getOnlyFinishedSession();
        Log.v(mTag, "session id: " + session.id);

        assertNoEvents(session, OutOfProcessActivity.COMPONENT_NAME);
    }
}
