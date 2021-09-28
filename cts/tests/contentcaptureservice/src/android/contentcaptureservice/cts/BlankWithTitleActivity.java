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

import static android.contentcaptureservice.cts.Assertions.assertDecorViewAppeared;
import static android.contentcaptureservice.cts.Assertions.assertRightActivity;
import static android.contentcaptureservice.cts.Assertions.assertSessionPaused;
import static android.contentcaptureservice.cts.Assertions.assertSessionResumed;
import static android.contentcaptureservice.cts.Assertions.assertViewAppeared;
import static android.contentcaptureservice.cts.Assertions.assertViewTreeFinished;
import static android.contentcaptureservice.cts.Assertions.assertViewTreeStarted;
import static android.contentcaptureservice.cts.Assertions.assertViewsOptionallyDisappeared;

import static com.google.common.truth.Truth.assertThat;

import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.util.Log;
import android.view.View;
import android.view.contentcapture.ContentCaptureEvent;
import android.view.contentcapture.ContentCaptureSessionId;
import android.view.contentcapture.ViewNode;

import androidx.annotation.NonNull;

import java.util.List;

public class BlankWithTitleActivity extends AbstractContentCaptureActivity {

    private static final String TAG = BlankWithTitleActivity.class.getSimpleName();

    @Override
    public void assertDefaultEvents(@NonNull Session session) {
        final ContentCaptureSessionId sessionId = session.id;
        assertRightActivity(session, sessionId, this);

        final View decorView = getDecorView();

        final List<ContentCaptureEvent> events = session.getEvents();
        Log.v(TAG, "events(" + events.size() + "): " + events);

        final int minEvents = 9; // TODO(b/122315042): disappeared not always sent
        assertThat(events.size()).isAtLeast(minEvents);

        assertSessionResumed(events, 0);
        assertViewTreeStarted(events, 1);
        assertDecorViewAppeared(events, 2, decorView);
        // TODO(b/123540067): ignoring 3 intermediate parents
        final ViewNode title = assertViewAppeared(events, 6).getViewNode();
        assertThat(title.getText()).isEqualTo("Blanka");
        assertViewTreeFinished(events, 7);
        assertSessionPaused(events, 8);
        if (false) { // TODO(b/123540067): disabled because it includes the parent
            assertViewsOptionallyDisappeared(events, minEvents, decorView.getAutofillId(),
                    title.getAutofillId());
        }
    }
}
