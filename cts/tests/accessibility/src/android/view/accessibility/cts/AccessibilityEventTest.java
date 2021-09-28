/*
 * Copyright (C) 2010 The Android Open Source Project
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

package android.view.accessibility.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.accessibility.cts.common.InstrumentedAccessibilityServiceTestRule;
import android.app.Activity;
import android.content.Context;
import android.os.Message;
import android.os.Parcel;
import android.platform.test.annotations.Presubmit;
import android.text.TextUtils;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityRecord;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;
import androidx.test.rule.ActivityTestRule;
import androidx.test.runner.AndroidJUnit4;

import junit.framework.TestCase;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

/**
 * Class for testing {@link AccessibilityEvent}.
 */
@Presubmit
@RunWith(AndroidJUnit4.class)
public class AccessibilityEventTest {

    private EventReportingLinearLayout mParentView;
    private View mChildView;
    private AccessibilityManager mAccessibilityManager;

    private final ActivityTestRule<DummyActivity> mActivityRule =
            new ActivityTestRule<>(DummyActivity.class, false, false);
    private final AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();
    private InstrumentedAccessibilityServiceTestRule<SpeakingAccessibilityService>
            mInstrumentedAccessibilityServiceRule = new InstrumentedAccessibilityServiceTestRule<>(
            SpeakingAccessibilityService.class, false);

    @Rule
    public final RuleChain mRuleChain = RuleChain
            .outerRule(mActivityRule)
            .around(mInstrumentedAccessibilityServiceRule)
            .around(mDumpOnFailureRule);

    @Before
    public void setUp() throws Throwable {
        final Activity activity = mActivityRule.launchActivity(null);
        mAccessibilityManager = activity.getSystemService(AccessibilityManager.class);
        mInstrumentedAccessibilityServiceRule.enableService();
        mActivityRule.runOnUiThread(() -> {
            final LinearLayout grandparent = new LinearLayout(activity);
            activity.setContentView(grandparent);
            mParentView = new EventReportingLinearLayout(activity);
            mChildView = new View(activity);
            grandparent.addView(mParentView);
            mParentView.addView(mChildView);
        });
    }

    private static class EventReportingLinearLayout extends LinearLayout {
        public List<AccessibilityEvent> mReceivedEvents = new ArrayList<AccessibilityEvent>();

        public EventReportingLinearLayout(Context context) {
            super(context);
        }

        @Override
        public boolean requestSendAccessibilityEvent(View child, AccessibilityEvent event) {
            mReceivedEvents.add(AccessibilityEvent.obtain(event));
            return super.requestSendAccessibilityEvent(child, event);
        }
    }

    @Test
    public void testScrollEvent() throws Exception {
        mChildView.scrollTo(0, 100);
        Thread.sleep(1000);
        scrollEventFilter.assertReceivedEventCount(1);
    }

    @Test
    public void testScrollEventBurstCombined() throws Exception {
        mChildView.scrollTo(0, 100);
        mChildView.scrollTo(0, 125);
        mChildView.scrollTo(0, 150);
        mChildView.scrollTo(0, 175);
        Thread.sleep(1000);
        scrollEventFilter.assertReceivedEventCount(1);
    }

    @Test
    public void testScrollEventsDeliveredInCorrectInterval() throws Exception {
        mChildView.scrollTo(0, 25);
        mChildView.scrollTo(0, 50);
        mChildView.scrollTo(0, 100);
        Thread.sleep(150);
        mChildView.scrollTo(0, 150);
        mChildView.scrollTo(0, 175);
        Thread.sleep(50);
        mChildView.scrollTo(0, 200);
        Thread.sleep(1000);
        scrollEventFilter.assertReceivedEventCount(2);
    }

    private AccessibilityEventFilter scrollEventFilter = new AccessibilityEventFilter() {
        public boolean pass(AccessibilityEvent event) {
            return event.getEventType() == AccessibilityEvent.TYPE_VIEW_SCROLLED;
        }
    };

    @Test
    public void testScrollEventsClearedOnDetach() throws Throwable {
        mChildView.scrollTo(0, 25);
        mChildView.scrollTo(5, 50);
        mChildView.scrollTo(7, 100);
        mActivityRule.runOnUiThread(() -> {
            mParentView.removeView(mChildView);
            mParentView.addView(mChildView);
        });
        mChildView.scrollTo(0, 150);
        Thread.sleep(1000);
        AccessibilityEvent event = scrollEventFilter.getLastEvent();
        assertEquals(-7, event.getScrollDeltaX());
        assertEquals(50, event.getScrollDeltaY());
    }

    @Test
    public void testScrollEventsCaptureTotalDelta() throws Throwable {
        mChildView.scrollTo(0, 25);
        mChildView.scrollTo(5, 50);
        mChildView.scrollTo(7, 100);
        Thread.sleep(1000);
        AccessibilityEvent event = scrollEventFilter.getLastEvent();
        assertEquals(7, event.getScrollDeltaX());
        assertEquals(100, event.getScrollDeltaY());
    }

    @Test
    public void testScrollEventsClearDeltaAfterSending() throws Throwable {
        mChildView.scrollTo(0, 25);
        mChildView.scrollTo(5, 50);
        mChildView.scrollTo(7, 100);
        Thread.sleep(1000);
        mChildView.scrollTo(0, 150);
        Thread.sleep(1000);
        AccessibilityEvent event = scrollEventFilter.getLastEvent();
        assertEquals(-7, event.getScrollDeltaX());
        assertEquals(50, event.getScrollDeltaY());
    }

    @Test
    public void testStateEvent() throws Throwable {
        sendStateDescriptionChangedEvent(mChildView);
        Thread.sleep(1000);
        stateDescriptionEventFilter.assertReceivedEventCount(1);
    }

    @Test
    public void testStateEventBurstCombined() throws Throwable {
        sendStateDescriptionChangedEvent(mChildView);
        sendStateDescriptionChangedEvent(mChildView);
        sendStateDescriptionChangedEvent(mChildView);
        sendStateDescriptionChangedEvent(mChildView);
        Thread.sleep(1000);
        stateDescriptionEventFilter.assertReceivedEventCount(1);
    }

    @Test
    public void testStateEventsDeliveredInCorrectInterval() throws Throwable {
        sendStateDescriptionChangedEvent(mChildView);
        sendStateDescriptionChangedEvent(mChildView);
        sendStateDescriptionChangedEvent(mChildView);
        Thread.sleep(150);
        sendStateDescriptionChangedEvent(mChildView);
        sendStateDescriptionChangedEvent(mChildView);
        Thread.sleep(50);
        sendStateDescriptionChangedEvent(mChildView);
        Thread.sleep(1000);
        stateDescriptionEventFilter.assertReceivedEventCount(2);
    }

    @Test
    public void testStateEventsHaveLastEventText() throws Throwable {
        sendStateDescriptionChangedEvent(mChildView, "First state");
        String expectedState = "Second state";
        sendStateDescriptionChangedEvent(mChildView, expectedState);
        Thread.sleep(1000);
        AccessibilityEvent event = stateDescriptionEventFilter.getLastEvent();
        assertEquals(expectedState, event.getText().get(0));
    }

    private AccessibilityEventFilter stateDescriptionEventFilter = new AccessibilityEventFilter() {
        public boolean pass(AccessibilityEvent event) {
            return event.getContentChangeTypes()
                    == AccessibilityEvent.CONTENT_CHANGE_TYPE_STATE_DESCRIPTION;
        }
    };

    private abstract class AccessibilityEventFilter {
        abstract boolean pass(AccessibilityEvent event);

        void assertReceivedEventCount(int count) {
            assertEquals(count, filteredEventsReceived().size());
        }

        AccessibilityEvent getLastEvent() {
            List<AccessibilityEvent> events = filteredEventsReceived();
            if (events.size() > 0) {
                return events.get(events.size() - 1);
            }
            return null;
        }

        private List<AccessibilityEvent> filteredEventsReceived() {
            List<AccessibilityEvent> filteredEvents = new ArrayList<AccessibilityEvent>();
            List<AccessibilityEvent> receivedEvents = mParentView.mReceivedEvents;
            for (int i = 0; i < receivedEvents.size(); i++) {
                if (pass(receivedEvents.get(i))) {
                    filteredEvents.add(receivedEvents.get(i));
                }
            }
            return filteredEvents;
        }
    }

    private void sendStateDescriptionChangedEvent(View view) {
        sendStateDescriptionChangedEvent(view, null);
    }

    private void sendStateDescriptionChangedEvent(View view, CharSequence text) {
        AccessibilityEvent event = AccessibilityEvent.obtain(
                AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        event.setContentChangeTypes(AccessibilityEvent.CONTENT_CHANGE_TYPE_STATE_DESCRIPTION);
        event.getText().add(text);
        view.sendAccessibilityEventUnchecked(event);
    }

    /**
     * Tests whether accessibility events are correctly written and
     * read from a parcel (version 1).
     */
    @SmallTest
    @Test
    public void testMarshaling() throws Exception {
        // fully populate the event to marshal
        AccessibilityEvent sentEvent = AccessibilityEvent.obtain();
        fullyPopulateAccessibilityEvent(sentEvent);

        // marshal and unmarshal the event
        Parcel parcel = Parcel.obtain();
        sentEvent.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        AccessibilityEvent receivedEvent = AccessibilityEvent.CREATOR.createFromParcel(parcel);

        // make sure all fields properly marshaled
        assertEqualsAccessibilityEvent(sentEvent, receivedEvent);

        parcel.recycle();
    }

    /**
     * Tests if {@link AccessibilityEvent} are properly reused.
     */
    @SmallTest
    @Test
    public void testReuse() {
        AccessibilityEvent firstEvent = AccessibilityEvent.obtain();
        firstEvent.recycle();
        AccessibilityEvent secondEvent = AccessibilityEvent.obtain();
        assertSame("AccessibilityEvent not properly reused", firstEvent, secondEvent);
    }

    /**
     * Tests if {@link AccessibilityEvent} are properly recycled.
     */
    @SmallTest
    @Test
    public void testRecycle() {
        // obtain and populate an event
        AccessibilityEvent populatedEvent = AccessibilityEvent.obtain();
        fullyPopulateAccessibilityEvent(populatedEvent);

        // recycle and obtain the same recycled instance
        populatedEvent.recycle();
        AccessibilityEvent recycledEvent = AccessibilityEvent.obtain();

        // check expectations
        assertAccessibilityEventCleared(recycledEvent);
    }

    /**
     * Tests whether the event types are correctly converted to strings.
     */
    @SmallTest
    @Test
    public void testEventTypeToString() {
        assertEquals("TYPE_NOTIFICATION_STATE_CHANGED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_NOTIFICATION_STATE_CHANGED));
        assertEquals("TYPE_TOUCH_EXPLORATION_GESTURE_END", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_END));
        assertEquals("TYPE_TOUCH_EXPLORATION_GESTURE_START", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_TOUCH_EXPLORATION_GESTURE_START));
        assertEquals("TYPE_VIEW_CLICKED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_VIEW_CLICKED));
        assertEquals("TYPE_VIEW_FOCUSED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_VIEW_FOCUSED));
        assertEquals("TYPE_VIEW_HOVER_ENTER",
                AccessibilityEvent.eventTypeToString(AccessibilityEvent.TYPE_VIEW_HOVER_ENTER));
        assertEquals("TYPE_VIEW_HOVER_EXIT", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_VIEW_HOVER_EXIT));
        assertEquals("TYPE_VIEW_LONG_CLICKED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_VIEW_LONG_CLICKED));
        assertEquals("TYPE_VIEW_CONTEXT_CLICKED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_VIEW_CONTEXT_CLICKED));
        assertEquals("TYPE_VIEW_SCROLLED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_VIEW_SCROLLED));
        assertEquals("TYPE_VIEW_SELECTED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_VIEW_SELECTED));
        assertEquals("TYPE_VIEW_TEXT_CHANGED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_VIEW_TEXT_CHANGED));
        assertEquals("TYPE_VIEW_TEXT_SELECTION_CHANGED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_VIEW_TEXT_SELECTION_CHANGED));
        assertEquals("TYPE_WINDOW_CONTENT_CHANGED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED));
        assertEquals("TYPE_WINDOW_STATE_CHANGED", AccessibilityEvent.eventTypeToString(
                AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED));
    }

    /**
     * Tests whether the event describes its contents consistently.
     */
    @SmallTest
    @Test
    public void testDescribeContents() {
        AccessibilityEvent event = AccessibilityEvent.obtain();
        assertSame("Accessibility events always return 0 for this method.", 0,
                event.describeContents());
        fullyPopulateAccessibilityEvent(event);
        assertSame("Accessibility events always return 0 for this method.", 0,
                event.describeContents());
    }

    /**
     * Tests whether accessibility events are correctly written and
     * read from a parcel (version 2).
     */
    @SmallTest
    @Test
    public void testMarshaling2() {
        // fully populate the event to marshal
        AccessibilityEvent marshaledEvent = AccessibilityEvent.obtain();
        fullyPopulateAccessibilityEvent(marshaledEvent);

        // marshal and unmarshal the event
        Parcel parcel = Parcel.obtain();
        marshaledEvent.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        AccessibilityEvent unmarshaledEvent = AccessibilityEvent.obtain();
        unmarshaledEvent.initFromParcel(parcel);

        // make sure all fields properly marshaled
        assertEqualsAccessibilityEvent(marshaledEvent, unmarshaledEvent);

        parcel.recycle();
    }

    /**
     * While CharSequence is immutable, some classes implementing it are mutable. Make sure they
     * can't change the object by changing the objects backing CharSequence
     */
    @SmallTest
    @Test
    public void testChangeTextAfterSetting_shouldNotAffectEvent() {
        final String originalText = "Cassowary";
        final String newText = "Hornbill";
        AccessibilityEvent event = AccessibilityEvent.obtain();
        StringBuffer updatingString = new StringBuffer(originalText);
        event.setBeforeText(updatingString);
        event.setContentDescription(updatingString);

        updatingString.delete(0, updatingString.length());
        updatingString.append(newText);

        assertTrue(TextUtils.equals(originalText, event.getBeforeText()));
        assertTrue(TextUtils.equals(originalText, event.getContentDescription()));
    }

    @SmallTest
    @Test
    public void testConstructors() {
        final AccessibilityEvent populatedEvent = new AccessibilityEvent();
        fullyPopulateAccessibilityEvent(populatedEvent);
        final AccessibilityEvent event = new AccessibilityEvent(populatedEvent);

        assertEqualsAccessibilityEvent(event, populatedEvent);

        final AccessibilityEvent firstEvent = new AccessibilityEvent();
        firstEvent.setEventType(AccessibilityEvent.TYPE_VIEW_FOCUSED);
        final AccessibilityEvent secondEvent = new AccessibilityEvent(
                AccessibilityEvent.TYPE_VIEW_FOCUSED);

        assertEqualsAccessibilityEvent(firstEvent, secondEvent);
    }

    /**
     * Fully populates the {@link AccessibilityEvent} to marshal.
     *
     * @param sentEvent The event to populate.
     */
    private void fullyPopulateAccessibilityEvent(AccessibilityEvent sentEvent) {
        sentEvent.setAddedCount(1);
        sentEvent.setBeforeText("BeforeText");
        sentEvent.setChecked(true);
        sentEvent.setClassName("foo.bar.baz.Class");
        sentEvent.setContentDescription("ContentDescription");
        sentEvent.setCurrentItemIndex(1);
        sentEvent.setEnabled(true);
        sentEvent.setEventType(AccessibilityEvent.TYPE_VIEW_FOCUSED);
        sentEvent.setEventTime(1000);
        sentEvent.setFromIndex(1);
        sentEvent.setFullScreen(true);
        sentEvent.setItemCount(1);
        sentEvent.setPackageName("foo.bar.baz");
        sentEvent.setParcelableData(Message.obtain(null, 1, 2, 3));
        sentEvent.setPassword(true);
        sentEvent.setRemovedCount(1);
        sentEvent.getText().add("Foo");
        sentEvent.setMaxScrollX(1);
        sentEvent.setMaxScrollY(1);
        sentEvent.setScrollX(1);
        sentEvent.setScrollY(1);
        sentEvent.setScrollDeltaX(3);
        sentEvent.setScrollDeltaY(3);
        sentEvent.setToIndex(1);
        sentEvent.setScrollable(true);
        sentEvent.setAction(AccessibilityNodeInfo.ACTION_ACCESSIBILITY_FOCUS);
        sentEvent.setMovementGranularity(AccessibilityNodeInfo.MOVEMENT_GRANULARITY_LINE);

        AccessibilityRecord record = AccessibilityRecord.obtain();
        AccessibilityRecordTest.fullyPopulateAccessibilityRecord(record);
        sentEvent.appendRecord(record);
    }

    /**
     * Compares all properties of the <code>expectedEvent</code> and the
     * <code>receivedEvent</code> to verify that the received event is the one
     * that is expected.
     */
    private static void assertEqualsAccessibilityEvent(AccessibilityEvent expectedEvent,
            AccessibilityEvent receivedEvent) {
        assertEquals("addedCount has incorrect value", expectedEvent.getAddedCount(), receivedEvent
                .getAddedCount());
        assertEquals("beforeText has incorrect value", expectedEvent.getBeforeText(), receivedEvent
                .getBeforeText());
        assertEquals("checked has incorrect value", expectedEvent.isChecked(), receivedEvent
                .isChecked());
        assertEquals("className has incorrect value", expectedEvent.getClassName(), receivedEvent
                .getClassName());
        assertEquals("contentDescription has incorrect value", expectedEvent
                .getContentDescription(), receivedEvent.getContentDescription());
        assertEquals("currentItemIndex has incorrect value", expectedEvent.getCurrentItemIndex(),
                receivedEvent.getCurrentItemIndex());
        assertEquals("enabled has incorrect value", expectedEvent.isEnabled(), receivedEvent
                .isEnabled());
        assertEquals("eventType has incorrect value", expectedEvent.getEventType(), receivedEvent
                .getEventType());
        assertEquals("fromIndex has incorrect value", expectedEvent.getFromIndex(), receivedEvent
                .getFromIndex());
        assertEquals("fullScreen has incorrect value", expectedEvent.isFullScreen(), receivedEvent
                .isFullScreen());
        assertEquals("itemCount has incorrect value", expectedEvent.getItemCount(), receivedEvent
                .getItemCount());
        assertEquals("password has incorrect value", expectedEvent.isPassword(), receivedEvent
                .isPassword());
        assertEquals("removedCount has incorrect value", expectedEvent.getRemovedCount(),
                receivedEvent.getRemovedCount());
        assertSame("maxScrollX has incorrect value", expectedEvent.getMaxScrollX(),
                receivedEvent.getMaxScrollX());
        assertSame("maxScrollY has incorrect value", expectedEvent.getMaxScrollY(),
                receivedEvent.getMaxScrollY());
        assertSame("scrollX has incorrect value", expectedEvent.getScrollX(),
                receivedEvent.getScrollX());
        assertSame("scrollY has incorrect value", expectedEvent.getScrollY(),
                receivedEvent.getScrollY());
        assertSame("scrollDeltaX has incorrect value", expectedEvent.getScrollDeltaX(),
                receivedEvent.getScrollDeltaX());
        assertSame("scrollDeltaY has incorrect value", expectedEvent.getScrollDeltaY(),
                receivedEvent.getScrollDeltaY());
        assertSame("toIndex has incorrect value", expectedEvent.getToIndex(),
                receivedEvent.getToIndex());
        assertSame("scrollable has incorrect value", expectedEvent.isScrollable(),
                receivedEvent.isScrollable());
        assertSame("granularity has incorrect value", expectedEvent.getMovementGranularity(),
                receivedEvent.getMovementGranularity());
        assertSame("action has incorrect value", expectedEvent.getAction(),
                receivedEvent.getAction());
        assertSame("windowChangeTypes has incorrect value", expectedEvent.getWindowChanges(),
                receivedEvent.getWindowChanges());

        AccessibilityRecordTest.assertEqualsText(expectedEvent.getText(), receivedEvent.getText());
        AccessibilityRecordTest.assertEqualAccessibilityRecord(expectedEvent, receivedEvent);

        assertEqualAppendedRecord(expectedEvent, receivedEvent);
    }

    private static void assertEqualAppendedRecord(AccessibilityEvent expectedEvent,
            AccessibilityEvent receivedEvent) {
        assertEquals("recordCount has incorrect value", expectedEvent.getRecordCount(),
                receivedEvent.getRecordCount());
        if (expectedEvent.getRecordCount() != 0 && receivedEvent.getRecordCount() != 0) {
            AccessibilityRecord expectedRecord =  expectedEvent.getRecord(0);
            AccessibilityRecord receivedRecord = receivedEvent.getRecord(0);
            AccessibilityRecordTest.assertEqualAccessibilityRecord(expectedRecord, receivedRecord);
        }
    }

    /**
     * Asserts that an {@link AccessibilityEvent} is cleared.
     *
     * @param event The event to check.
     */
    private static void assertAccessibilityEventCleared(AccessibilityEvent event) {
        AccessibilityRecordTest.assertAccessibilityRecordCleared(event);
        TestCase.assertEquals("eventTime not properly recycled", 0, event.getEventTime());
        TestCase.assertEquals("eventType not properly recycled", 0, event.getEventType());
        TestCase.assertNull("packageName not properly recycled", event.getPackageName());
    }
}
