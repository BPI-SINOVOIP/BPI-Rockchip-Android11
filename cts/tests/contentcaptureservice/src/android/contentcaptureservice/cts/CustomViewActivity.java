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
import static android.contentcaptureservice.cts.Assertions.assertViewWithUnknownParentAppeared;
import static android.contentcaptureservice.cts.Assertions.assertViewsOptionallyDisappeared;

import static com.google.common.truth.Truth.assertThat;

import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.ViewStructure;
import android.view.contentcapture.ContentCaptureEvent;

import androidx.annotation.NonNull;

import com.android.compatibility.common.util.DoubleVisitor;
import com.android.compatibility.common.util.Visitor;

import java.util.List;

public class CustomViewActivity extends AbstractContentCaptureActivity {

    private static final String TAG = CustomViewActivity.class.getSimpleName();

    private static DoubleVisitor<CustomView, ViewStructure> sCustomViewDelegate;
    private static Visitor<CustomViewActivity> sRootViewVisitor;

    /**
     * Mininum number of events generated when the activity starts.
     *
     * <p>Used on {@link #assertInitialViewsAppeared(Session, int)} and
     * {@link #assertInitialViewsDisappeared(List, int)}.
     */
    public static final int MIN_EVENTS = 7;

    CustomView mCustomView;

    /**
     * Sets a delegate that provides the behavior of
     * {@link CustomView#onProvideContentCaptureStructure(ViewStructure, int)}.
     */
    static void setCustomViewDelegate(@NonNull DoubleVisitor<CustomView, ViewStructure> delegate) {
        sCustomViewDelegate = delegate;
    }

    static void onRootView(@NonNull Visitor<CustomViewActivity> visitor) {
        sRootViewVisitor = visitor;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.custom_view_activity);
        mCustomView = findViewById(R.id.custom_view);
        Log.d(TAG, "onCreate(): custom view id is " + mCustomView.getAutofillId());
        if (sCustomViewDelegate != null) {
            Log.d(TAG, "Setting delegate on " + mCustomView);
            mCustomView.setContentCaptureDelegate(
                    (structure) -> sCustomViewDelegate.visit(mCustomView, structure));
        }
        if (sRootViewVisitor != null) {
            try {
                sRootViewVisitor.visit(this);
            } finally {
                sRootViewVisitor = null;
            }
        }
    }

    @Override
    public void assertDefaultEvents(@NonNull Session session) {
        assertRightActivity(session, session.id, this);
        final int additionalEvents = 0;
        final List<ContentCaptureEvent> events = assertInitialViewsAppeared(session,
                additionalEvents);
        Log.v(TAG, "events(" + events.size() + "): " + events);
        assertInitialViewsDisappeared(events, additionalEvents);
    }

    /**
     * Asserts the events generated when this activity was launched, up to the
     * {@code TYPE_INITIAL_VIEW_HIERARCHY_FINISHED} event.
     */
    @NonNull
    public List<ContentCaptureEvent> assertInitialViewsAppeared(Session session,
            int additionalEvents) {
        return assertJustInitialViewsAppeared(session, additionalEvents);
    }

    /**
     * Asserts the events generated when this activity was launched, but without the
     * {@code TYPE_INITIAL_VIEW_HIERARCHY_FINISHED} event.
     */
    @NonNull
    private List<ContentCaptureEvent> assertJustInitialViewsAppeared(@NonNull Session session,
            int additionalEvents) {
        final View grandpa1 = (View) mCustomView.getParent();
        final View grandpa2 = (View) grandpa1.getParent();
        final View decorView = getDecorView();
        Log.v(TAG, "assertJustInitialViewsAppeared(): grandpa1=" + grandpa1.getAutofillId()
                + ", grandpa2=" + grandpa2.getAutofillId() + ", decor="
                + decorView.getAutofillId());

        final List<ContentCaptureEvent> events = session.getEvents();
        Log.v(TAG, "events(" + events.size() + "): " + events);
        assertThat(events.size()).isAtLeast(MIN_EVENTS + additionalEvents);

        // Assert just the relevant events
        assertSessionResumed(events, 0);
        assertViewTreeStarted(events, 1);
        assertDecorViewAppeared(events, 2, getDecorView());
        assertViewAppeared(events, 3, grandpa2, decorView.getAutofillId());
        assertViewAppeared(events, 4, grandpa1, grandpa2.getAutofillId());
        assertViewWithUnknownParentAppeared(events, 5, session.id, mCustomView);
        assertViewTreeFinished(events, 6);

        return events;
    }

    /**
     * Asserts the initial views disappeared after the activity was finished.
     */
    // TODO(b/123540067, 122315042): fix and document or remove
    public void assertInitialViewsDisappeared(@NonNull List<ContentCaptureEvent> events,
            int additionalEvents) {
        if (true) return;     // TODO(b/123540067, 122315042): not really working
        assertViewsOptionallyDisappeared(events, MIN_EVENTS + additionalEvents,
                getDecorView().getAutofillId(), mCustomView.getAutofillId());
        assertSessionPaused(events, MIN_EVENTS + additionalEvents);
    }
}
