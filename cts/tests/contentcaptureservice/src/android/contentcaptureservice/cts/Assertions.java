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

import static android.contentcaptureservice.cts.Helper.MY_EPOCH;
import static android.contentcaptureservice.cts.Helper.TAG;
import static android.view.contentcapture.ContentCaptureEvent.TYPE_CONTEXT_UPDATED;
import static android.view.contentcapture.ContentCaptureEvent.TYPE_SESSION_PAUSED;
import static android.view.contentcapture.ContentCaptureEvent.TYPE_SESSION_RESUMED;
import static android.view.contentcapture.ContentCaptureEvent.TYPE_VIEW_APPEARED;
import static android.view.contentcapture.ContentCaptureEvent.TYPE_VIEW_DISAPPEARED;
import static android.view.contentcapture.ContentCaptureEvent.TYPE_VIEW_INSETS_CHANGED;
import static android.view.contentcapture.ContentCaptureEvent.TYPE_VIEW_TEXT_CHANGED;
import static android.view.contentcapture.ContentCaptureEvent.TYPE_VIEW_TREE_APPEARED;
import static android.view.contentcapture.ContentCaptureEvent.TYPE_VIEW_TREE_APPEARING;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.content.ComponentName;
import android.content.LocusId;
import android.contentcaptureservice.cts.CtsContentCaptureService.Session;
import android.util.Log;
import android.view.Display;
import android.view.View;
import android.view.autofill.AutofillId;
import android.view.contentcapture.ContentCaptureEvent;
import android.view.contentcapture.ContentCaptureSession;
import android.view.contentcapture.ContentCaptureSessionId;
import android.view.contentcapture.ViewNode;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.compatibility.common.util.RetryableException;

import java.util.List;

/**
 * Helper for common assertions.
 */
final class Assertions {

    /**
     * Asserts a session belongs to the right activity.
     */
    public static void assertRightActivity(@NonNull Session session,
            @NonNull ContentCaptureSessionId expectedSessionId,
            @NonNull AbstractContentCaptureActivity activity) {
        assertRightActivity(session, expectedSessionId, activity.getComponentName());
    }

    /**
     * Asserts a session belongs to the right activity.
     */
    public static void assertRightActivity(@NonNull Session session,
            @NonNull ContentCaptureSessionId expectedSessionId,
            @NonNull ComponentName componentName) {
        assertWithMessage("wrong activity for %s", session)
                .that(session.context.getActivityComponent()).isEqualTo(componentName);
        // TODO(b/123540602): merge both or replace check above by:
        //  assertMainSessionContext(session, activity);
        assertThat(session.id).isEqualTo(expectedSessionId);
    }

    /**
     * Asserts the context of a main session.
     */
    public static void assertMainSessionContext(@NonNull Session session,
            @NonNull AbstractContentCaptureActivity activity) {
        assertMainSessionContext(session, activity, /* expectedFlags= */ 0);
    }

    /**
     * Asserts the context of a main session.
     */
    public static void assertMainSessionContext(@NonNull Session session,
            @NonNull AbstractContentCaptureActivity activity, int expectedFlags) {
        assertWithMessage("no context on %s", session).that(session.context).isNotNull();
        assertWithMessage("wrong activity for %s", session)
                .that(session.context.getActivityComponent())
                .isEqualTo(activity.getComponentName());
        assertWithMessage("context for session %s should have displayId", session)
                .that(session.context.getDisplayId()).isNotEqualTo(Display.INVALID_DISPLAY);
        assertWithMessage("wrong task id for session %s", session)
                .that(session.context.getTaskId()).isEqualTo(activity.getRealTaskId());
        assertWithMessage("wrong flags on context for session %s", session)
                .that(session.context.getFlags()).isEqualTo(expectedFlags);
        assertWithMessage("context for session %s should not have ID", session)
                .that(session.context.getLocusId()).isNull();
        assertWithMessage("context for session %s should not have extras", session)
                .that(session.context.getExtras()).isNull();
    }

    /**
     * Asserts the invariants of a child session.
     */
    public static void assertChildSessionContext(@NonNull Session session) {
        assertWithMessage("no context on %s", session).that(session.context).isNotNull();
        assertWithMessage("context for session %s should not have component", session)
                .that(session.context.getActivityComponent()).isNull();
        assertWithMessage("context for session %s should not have displayId", session)
                .that(session.context.getDisplayId()).isEqualTo(Display.INVALID_DISPLAY);
        assertWithMessage("context for session %s should not have taskId", session)
                .that(session.context.getTaskId()).isEqualTo(0);
        assertWithMessage("context for session %s should not have flags", session)
                .that(session.context.getFlags()).isEqualTo(0);
    }

    /**
     * Asserts the invariants of a child session.
     */
    public static void assertChildSessionContext(@NonNull Session session,
            @NonNull String expectedId) {
        assertChildSessionContext(session);
        assertThat(session.context.getLocusId()).isEqualTo(new LocusId(expectedId));
    }

    /**
     * Asserts a session belongs to the right parent
     */
    public static void assertRightRelationship(@NonNull Session parent, @NonNull Session child) {
        final ContentCaptureSessionId expectedParentId = parent.id;
        assertWithMessage("No id on parent session %s", parent).that(expectedParentId).isNotNull();
        assertWithMessage("No context on child session %s", child).that(child.context).isNotNull();
        final ContentCaptureSessionId actualParentId = child.context.getParentSessionId();
        assertWithMessage("No parent id on context %s of child session %s", child.context, child)
                .that(actualParentId).isNotNull();
        assertWithMessage("id of parent session doesn't match child").that(actualParentId)
                .isEqualTo(expectedParentId);
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_APPEARED} event, without checking for parent id.
     */
    public static ViewNode assertViewWithUnknownParentAppeared(
            @NonNull List<ContentCaptureEvent> events, int index, @NonNull View expectedView) {
        return assertViewWithUnknownParentAppeared(events, index, expectedView,
                /* expectedText= */ null);
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_APPEARED} event for a decor view.
     *
     * <P>The decor view is typically internal, so there isn't much we can assert, other than its
     * autofill id.
     */
    public static void assertDecorViewAppeared(@NonNull List<ContentCaptureEvent> events,
            int index, @NonNull View expectedDecorView) {
        final ContentCaptureEvent event = assertViewAppeared(events, index);
        assertWithMessage("wrong autofill id on %s (%s)", event, index)
                .that(event.getViewNode().getAutofillId())
                .isEqualTo(expectedDecorView.getAutofillId());
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_APPEARED} event, without checking for parent id.
     */
    public static ViewNode assertViewWithUnknownParentAppeared(
            @NonNull List<ContentCaptureEvent> events, int index, @NonNull View expectedView,
            @Nullable String expectedText) {
        final ContentCaptureEvent event = assertViewAppeared(events, index);
        final ViewNode node = event.getViewNode();

        assertWithMessage("wrong class on %s (%s)", event, index).that(node.getClassName())
                .isEqualTo(expectedView.getClass().getName());
        assertWithMessage("wrong autofill id on %s (%s)", event, index).that(node.getAutofillId())
                .isEqualTo(expectedView.getAutofillId());

        if (expectedText != null) {
            assertWithMessage("wrong text on %s (%s)", event, index).that(node.getText().toString())
                    .isEqualTo(expectedText);
        }
        // TODO(b/123540602): test more fields, like resource id
        return node;
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_APPEARED} event, without checking for parent id.
     */
    public static ContentCaptureEvent assertViewAppeared(@NonNull List<ContentCaptureEvent> events,
            int index) {
        final ContentCaptureEvent event = getEvent(events, index, TYPE_VIEW_APPEARED);
        final ViewNode node = event.getViewNode();
        assertThat(node).isNotNull();
        return event;
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_APPEARED} event.
     */
    public static void assertViewAppeared(@NonNull List<ContentCaptureEvent> events, int index,
            @NonNull View expectedView, @Nullable AutofillId expectedParentId,
            @Nullable String expectedText) {
        final ViewNode node = assertViewWithUnknownParentAppeared(events, index, expectedView,
                expectedText);
        assertWithMessage("wrong parent autofill id on %s (%s)", events.get(index), index)
            .that(node.getParentAutofillId()).isEqualTo(expectedParentId);
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_APPEARED} event.
     */
    public static void assertViewAppeared(@NonNull List<ContentCaptureEvent> events, int index,
            @NonNull View expectedView, @Nullable AutofillId expectedParentId) {
        assertViewAppeared(events, index, expectedView, expectedParentId, /* expectedText= */ null);
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_APPEARED} event.
     */
    public static void assertViewAppeared(@NonNull List<ContentCaptureEvent> events, int index,
            @NonNull ContentCaptureSessionId expectedSessionId,
            @NonNull View expectedView, @Nullable AutofillId expectedParentId) {
        assertViewAppeared(events, index, expectedView, expectedParentId);
        assertSessionId(expectedSessionId, expectedView);
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_TREE_APPEARING} event.
     */
    public static void assertViewTreeStarted(@NonNull List<ContentCaptureEvent> events,
            int index) {
        assertSessionLevelEvent(events, index, TYPE_VIEW_TREE_APPEARING);
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_TREE_APPEARED} event.
     */
    public static void assertViewTreeFinished(@NonNull List<ContentCaptureEvent> events,
            int index) {
        assertSessionLevelEvent(events, index, TYPE_VIEW_TREE_APPEARED);
    }

    private static void assertSessionLevelEvent(@NonNull List<ContentCaptureEvent> events,
            int index, int expectedType) {
        final ContentCaptureEvent event = getEvent(events, index, expectedType);
        assertWithMessage("event %s (index %s) should not have a ViewNode", event, index)
                .that(event.getViewNode()).isNull();
        assertWithMessage("event %s (index %s) should not have text", event, index)
                .that(event.getViewNode()).isNull();
        assertWithMessage("event %s (index %s) should not have an autofillId", event, index)
                .that(event.getId()).isNull();
        assertWithMessage("event %s (index %s) should not have autofillIds", event, index)
                .that(event.getIds()).isNull();
    }

    /**
     * Asserts the contents of a {@link #TYPE_SESSION_RESUMED} event.
     */
    public static void assertSessionResumed(@NonNull List<ContentCaptureEvent> events,
            int index) {
        assertSessionLevelEvent(events, index, TYPE_SESSION_RESUMED);
    }

    /**
     * Asserts the contents of a {@link #TYPE_SESSION_PAUSED} event.
     */
    public static void assertSessionPaused(@NonNull List<ContentCaptureEvent> events,
            int index) {
        assertSessionLevelEvent(events, index, TYPE_SESSION_PAUSED);
    }

    /**
     * Asserts that a session for the given activity has no view-level events, just
     * {@link #TYPE_SESSION_RESUMED} and {@link #TYPE_SESSION_PAUSED}.
     */
    public static void assertNoViewLevelEvents(@NonNull Session session,
            @NonNull AbstractContentCaptureActivity activity) {
        assertRightActivity(session, session.id, activity);
        final List<ContentCaptureEvent> events = session.getEvents();
        Log.v(TAG, "events on " + activity + ": " + events);
        assertThat(events).hasSize(2);
        assertSessionResumed(events, 0);
        assertSessionPaused(events, 1);
    }

    /**
     * Asserts that a session for the given activity has events at all.
     */
    public static void assertNoEvents(@NonNull Session session,
            @NonNull ComponentName componentName) {
        assertRightActivity(session, session.id, componentName);
        assertThat(session.getEvents()).isEmpty();
    }

    /**
     * Asserts that the events received by the service optionally contains the
     * {@code TYPE_VIEW_DISAPPEARED} events, as they might have not been generated if the views
     * disappeared after the activity stopped.
     *
     * @param events events received by the service.
     * @param minimumSize size of events received if activity stopped before views disappeared
     * @param expectedIds ids of views that might have disappeared.
     *
     * @return whether the view disappeared events were generated
     */
    // TODO(b/123540067, 122315042): remove this method if we could make it deterministic, and
    // inline the assertions (or rename / change its logic)
    public static boolean assertViewsOptionallyDisappeared(
            @NonNull List<ContentCaptureEvent> events, int minimumSize,
            @NonNull AutofillId... expectedIds) {
        final int actualSize = events.size();
        final int disappearedEventIndex;
        if (actualSize == minimumSize) {
            // Activity stopped before TYPE_VIEW_DISAPPEARED were sent.
            return false;
        } else if (actualSize == minimumSize + 1) {
            // Activity did not receive TYPE_VIEW_TREE_APPEARING and TYPE_VIEW_TREE_APPEARED.
            disappearedEventIndex = minimumSize;
        } else {
            disappearedEventIndex = minimumSize + 1;
        }
        final ContentCaptureEvent batchDisappearEvent = events.get(disappearedEventIndex);

        if (expectedIds.length == 1) {
            assertWithMessage("Should have just one deleted id on %s", batchDisappearEvent)
                    .that(batchDisappearEvent.getIds()).isNull();
            assertWithMessage("wrong deleted id on %s", batchDisappearEvent)
                    .that(batchDisappearEvent.getId()).isEqualTo(expectedIds[0]);
        } else {
            assertWithMessage("Should not have individual deleted id on %s", batchDisappearEvent)
                    .that(batchDisappearEvent.getId()).isNull();
            final List<AutofillId> actualIds = batchDisappearEvent.getIds();
            assertWithMessage("wrong deleteds id on %s", batchDisappearEvent)
                    .that(actualIds).containsExactly((Object[]) expectedIds);
        }
        return true;
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_APPEARED} event, without checking for parent
     */
    public static void assertViewWithUnknownParentAppeared(
            @NonNull List<ContentCaptureEvent> events, int index,
            @NonNull ContentCaptureSessionId expectedSessionId, @NonNull View expectedView) {
        assertViewWithUnknownParentAppeared(events, index, expectedView);
        assertSessionId(expectedSessionId, expectedView);
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_DISAPPEARED} event for a single view.
     */
    public static void assertViewDisappeared(@NonNull List<ContentCaptureEvent> events, int index,
            @NonNull AutofillId expectedId) {
        final ContentCaptureEvent event = assertCommonViewDisappearedProperties(events, index);
        assertWithMessage("wrong autofillId on event %s (index %s)", event, index)
            .that(event.getId()).isEqualTo(expectedId);
        assertWithMessage("event %s (index %s) should not have autofillIds", event, index)
            .that(event.getIds()).isNull();
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_DISAPPEARED} event for multiple views.
     */
    public static void assertViewsDisappeared(@NonNull List<ContentCaptureEvent> events, int index,
            @NonNull AutofillId... expectedIds) {
        final ContentCaptureEvent event = assertCommonViewDisappearedProperties(events, index);
        final List<AutofillId> ids = event.getIds();
        assertWithMessage("no autofillIds on event %s (index %s)", event, index).that(ids)
                .isNotNull();
        assertWithMessage("wrong autofillId on event %s (index %s)", event, index)
            .that(ids).containsExactly((Object[]) expectedIds).inOrder();
        assertWithMessage("event %s (index %s) should not have autofillId", event, index)
            .that(event.getId()).isNull();
    }

    private static ContentCaptureEvent assertCommonViewDisappearedProperties(
            @NonNull List<ContentCaptureEvent> events, int index) {
        final ContentCaptureEvent event = getEvent(events, index, TYPE_VIEW_DISAPPEARED);
        assertWithMessage("event %s (index %s) should not have a ViewNode", event, index)
                .that(event.getViewNode()).isNull();
        assertWithMessage("event %s (index %s) should not have text", event, index)
                .that(event.getText()).isNull();
        return event;
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_APPEARED} event for a virtual node.
     */
    public static void assertVirtualViewAppeared(@NonNull List<ContentCaptureEvent> events,
            int index, @NonNull ContentCaptureSession session, @NonNull AutofillId parentId,
            int childId, @Nullable String expectedText) {
        final ContentCaptureEvent event = getEvent(events, index, TYPE_VIEW_APPEARED);
        final ViewNode node = event.getViewNode();
        assertThat(node).isNotNull();
        final AutofillId expectedId = session.newAutofillId(parentId, childId);
        assertWithMessage("wrong autofill id on %s (index %s)", event, index)
            .that(node.getAutofillId()).isEqualTo(expectedId);
        if (expectedText != null) {
            assertWithMessage("wrong text on %s(index %s) ", event, index)
                .that(node.getText().toString()).isEqualTo(expectedText);
        } else {
            assertWithMessage("%s (index %s) should not have text", node, index)
                .that(node.getText()).isNull();
        }
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_DISAPPEARED} event for a virtual node.
     */
    public static void assertVirtualViewDisappeared(@NonNull List<ContentCaptureEvent> events,
            int index, @NonNull AutofillId parentId, @NonNull ContentCaptureSession session,
            long childId) {
        assertViewDisappeared(events, index, session.newAutofillId(parentId, childId));
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_DISAPPEARED} event for many virtual nodes.
     */
    public static void assertVirtualViewsDisappeared(@NonNull List<ContentCaptureEvent> events,
            int index, @NonNull AutofillId parentId, @NonNull ContentCaptureSession session,
            long... childrenIds) {
        final int size = childrenIds.length;
        final AutofillId[] expectedIds = new AutofillId[size];
        for (int i = 0; i < childrenIds.length; i++) {
            expectedIds[i] = session.newAutofillId(parentId, childrenIds[i]);
        }
        assertViewsDisappeared(events, index, expectedIds);
    }

    /**
     * Asserts a view has the given session id.
     */
    public static void assertSessionId(@NonNull ContentCaptureSessionId expectedSessionId,
            @NonNull View view) {
        assertThat(expectedSessionId).isNotNull();
        final ContentCaptureSession session = view.getContentCaptureSession();
        assertWithMessage("no session for view %s", view).that(session).isNotNull();
        assertWithMessage("wrong session id for for view %s", view)
                .that(session.getContentCaptureSessionId()).isEqualTo(expectedSessionId);
    }

    /**
     * Asserts the contents of a {@link #TYPE_VIEW_TEXT_CHANGED} event.
     */
    public static void assertViewTextChanged(@NonNull List<ContentCaptureEvent> events, int index,
            @NonNull AutofillId expectedId, @NonNull String expectedText) {
        final ContentCaptureEvent event = getEvent(events, index, TYPE_VIEW_TEXT_CHANGED);
        assertWithMessage("Wrong id on %s (%s)", event, index).that(event.getId())
                .isEqualTo(expectedId);
        assertWithMessage("Wrong text on %s (%s)", event, index).that(event.getText().toString())
                .isEqualTo(expectedText);
    }

    /**
     * Asserts the existence and contents of a {@link #TYPE_VIEW_TEXT_CHANGED} event.
     */
    public static void assertViewInsetsChanged(@NonNull List<ContentCaptureEvent> events) {
        boolean insetsEventFound = false;
        for (ContentCaptureEvent event : events) {
            if (event.getType() == TYPE_VIEW_INSETS_CHANGED) {
                assertWithMessage("Expected view insets to be non-null on %s", event)
                    .that(event.getInsets()).isNotNull();
                insetsEventFound = true;
            }
        }

        if (!insetsEventFound) {
            throw new RetryableException(
                String.format(
                    "Expected at least one VIEW_INSETS_CHANGED event in the set of events %s",
                    events));
        }
    }

    /**
     * Asserts the basic contents of a {@link #TYPE_CONTEXT_UPDATED} event.
     */
    public static ContentCaptureEvent assertContextUpdated(
            @NonNull List<ContentCaptureEvent> events, int index) {
        final ContentCaptureEvent event = getEvent(events, index, TYPE_CONTEXT_UPDATED);
        assertWithMessage("event %s (index %s) should not have a ViewNode", event, index)
                .that(event.getViewNode()).isNull();
        assertWithMessage("event %s (index %s) should not have text", event, index)
                .that(event.getViewNode()).isNull();
        assertWithMessage("event %s (index %s) should not have an autofillId", event, index)
                .that(event.getId()).isNull();
        assertWithMessage("event %s (index %s) should not have autofillIds", event, index)
                .that(event.getIds()).isNull();
        return event;
    }

    /**
     * Asserts the order a session was created or destroyed.
     */
    public static void assertLifecycleOrder(int expectedOrder, @NonNull Session session,
            @NonNull LifecycleOrder type) {
        switch(type) {
            case CREATION:
                assertWithMessage("Wrong order of creation for session %s", session)
                    .that(session.creationOrder).isEqualTo(expectedOrder);
                break;
            case DESTRUCTION:
                assertWithMessage("Wrong order of destruction for session %s", session)
                    .that(session.destructionOrder).isEqualTo(expectedOrder);
                break;
            default:
                throw new IllegalArgumentException("Invalid type: " + type);
        }
    }

    /**
     * Gets the event at the given index, failing with the user-friendly message if necessary...
     */
    @NonNull
    private static ContentCaptureEvent getEvent(@NonNull List<ContentCaptureEvent> events,
            int index, int expectedType) {
        assertWithMessage("events is null").that(events).isNotNull();
        final ContentCaptureEvent event = events.get(index);
        assertWithMessage("no event at index %s (size %s): %s", index, events.size(), events)
                .that(event).isNotNull();
        final int actualType = event.getType();
        if (actualType != expectedType) {
            throw new AssertionError(String.format(
                    "wrong event type (expected %s, actual is %s) at index %s: %s",
                    eventTypeAsString(expectedType), eventTypeAsString(actualType), index, event));
        }
        assertWithMessage("invalid time on %s (index %s)", event, index).that(event.getEventTime())
                 .isAtLeast(MY_EPOCH);
        return event;
    }

    /**
     * Gets an user-friendly description of the given event type.
     */
    @NonNull
    public static String eventTypeAsString(int type) {
        final String string;
        switch (type) {
            case TYPE_VIEW_APPEARED:
                string = "APPEAR";
                break;
            case TYPE_VIEW_DISAPPEARED:
                string = "DISAPPEAR";
                break;
            case TYPE_VIEW_TEXT_CHANGED:
                string = "TEXT_CHANGE";
                break;
            case TYPE_VIEW_TREE_APPEARING:
                string = "TREE_START";
                break;
            case TYPE_VIEW_TREE_APPEARED:
                string = "TREE_END";
                break;
            case TYPE_SESSION_PAUSED:
                string = "PAUSED";
                break;
            case TYPE_SESSION_RESUMED:
                string = "RESUMED";
                break;
            default:
                return "UNKNOWN-" + type;
        }
        return String.format("%s-%d", string, type);
    }

    private Assertions() {
        throw new UnsupportedOperationException("contain static methods only");
    }

    public enum LifecycleOrder {
        CREATION, DESTRUCTION
    }
}
