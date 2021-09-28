/*
 * Copyright 2020 The Android Open Source Project
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

package com.android.car.calendar.common;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static java.time.temporal.ChronoUnit.HOURS;

import android.Manifest;
import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.os.Process;
import android.os.SystemClock;
import android.provider.CalendarContract;
import android.test.mock.MockContentProvider;
import android.test.mock.MockContentResolver;

import androidx.lifecycle.Observer;
import androidx.test.annotation.UiThreadTest;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.GrantPermissionRule;

import com.google.common.collect.ImmutableList;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.time.Clock;
import java.time.Duration;
import java.time.Instant;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.time.temporal.ChronoField;
import java.time.temporal.ChronoUnit;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@RunWith(AndroidJUnit4.class)
public class EventsLiveDataTest {
    private static final ZoneId BERLIN_ZONE_ID = ZoneId.of("Europe/Berlin");
    private static final ZonedDateTime CURRENT_DATE_TIME =
            LocalDateTime.of(2019, 12, 10, 10, 10, 10, 500500).atZone(BERLIN_ZONE_ID);
    private static final Dialer.NumberAndAccess EVENT_NUMBER_PIN =
            new Dialer.NumberAndAccess("the number", "the pin");
    private static final String EVENT_TITLE = "the title";
    private static final boolean EVENT_ALL_DAY = false;
    private static final String EVENT_LOCATION = "the location";
    private static final String EVENT_DESCRIPTION = "the description";
    private static final String CALENDAR_NAME = "the calendar name";
    private static final int CALENDAR_COLOR = 0xCAFEBABE;
    private static final int EVENT_ATTENDEE_STATUS =
            CalendarContract.Attendees.ATTENDEE_STATUS_ACCEPTED;

    @Rule
    public final GrantPermissionRule permissionRule =
            GrantPermissionRule.grant(Manifest.permission.READ_CALENDAR);

    private EventsLiveData mEventsLiveData;
    private TestContentProvider mTestContentProvider;
    private TestHandler mTestHandler;
    private TestClock mTestClock;

    @Before
    public void setUp() {
        mTestClock = new TestClock(BERLIN_ZONE_ID);
        mTestClock.setTime(CURRENT_DATE_TIME);
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        // Create a fake result for the calendar content provider.
        MockContentResolver mockContentResolver = new MockContentResolver(context);

        mTestContentProvider = new TestContentProvider(context);
        mockContentResolver.addProvider(CalendarContract.AUTHORITY, mTestContentProvider);

        EventDescriptions mockEventDescriptions = mock(EventDescriptions.class);
        when(mockEventDescriptions.extractNumberAndPins(any()))
                .thenReturn(ImmutableList.of(EVENT_NUMBER_PIN));

        EventLocations mockEventLocations = mock(EventLocations.class);
        when(mockEventLocations.isValidLocation(anyString())).thenReturn(true);
        mTestHandler = TestHandler.create();
        mEventsLiveData =
                new EventsLiveData(
                        mTestClock,
                        mTestHandler,
                        mockContentResolver,
                        mockEventDescriptions,
                        mockEventLocations);
    }

    @After
    public void tearDown() {
        if (mTestHandler != null) {
            mTestHandler.stop();
        }
    }

    @Test
    public void noObserver_noQueryMade() {
        // No query should be made because there are no observers.
        assertThat(mTestContentProvider.mTestEventCursor).isNull();
    }

    @Test
    @UiThreadTest
    public void addObserver_queryMade() throws InterruptedException {
        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);
        mEventsLiveData.observeForever((value) -> latch.countDown());

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        assertThat(mTestContentProvider.mTestEventCursor).isNotNull();
    }

    @Test
    @UiThreadTest
    public void addObserver_contentObserved() throws InterruptedException {
        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);
        mEventsLiveData.observeForever((value) -> latch.countDown());

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        assertThat(mTestContentProvider.mTestEventCursor.mLastContentObserver).isNotNull();
    }

    @Test
    @UiThreadTest
    public void removeObserver_contentNotObserved() throws InterruptedException {
        // Expect onChanged when we observe, when the data is read, and when we stop observing.
        final CountDownLatch latch = new CountDownLatch(2);
        Observer<ImmutableList<Event>> observer = (value) -> latch.countDown();
        mEventsLiveData.observeForever(observer);

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        final CountDownLatch latch2 = new CountDownLatch(1);
        mEventsLiveData.removeObserver(observer);

        // Wait for the observer to be unregistered on the background thread.
        latch2.await(5, TimeUnit.SECONDS);

        assertThat(mTestContentProvider.mTestEventCursor.mLastContentObserver).isNull();
    }

    @Test
    public void addObserver_oneEventResult() throws InterruptedException {

        mTestContentProvider.addRow(buildTestRowWithDuration(CURRENT_DATE_TIME, 1));

        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);

        // Must add observer on main thread.
        runOnMain(() -> mEventsLiveData.observeForever((value) -> latch.countDown()));

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        ImmutableList<Event> events = mEventsLiveData.getValue();
        assertThat(events).isNotNull();
        assertThat(events).hasSize(1);
        Event event = events.get(0);

        long eventStartMillis = addHoursAndTruncate(CURRENT_DATE_TIME, 0);
        long eventEndMillis = addHoursAndTruncate(CURRENT_DATE_TIME, 1);

        assertThat(event.getTitle()).isEqualTo(EVENT_TITLE);
        assertThat(event.getCalendarDetails().getColor()).isEqualTo(CALENDAR_COLOR);
        assertThat(event.getLocation()).isEqualTo(EVENT_LOCATION);
        assertThat(event.getStartInstant().toEpochMilli()).isEqualTo(eventStartMillis);
        assertThat(event.getEndInstant().toEpochMilli()).isEqualTo(eventEndMillis);
        assertThat(event.getStatus()).isEqualTo(Event.Status.ACCEPTED);
        assertThat(event.getNumberAndAccess()).isEqualTo(EVENT_NUMBER_PIN);
    }

    @Test
    public void changeCursorData_onChangedCalled() throws InterruptedException {
        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch initializeCountdownLatch = new CountDownLatch(2);

        // Expect the same init callbacks as above but with an extra when the data is updated.
        CountDownLatch changeCountdownLatch = new CountDownLatch(3);

        // Must add observer on main thread.
        runOnMain(
                () ->
                        mEventsLiveData.observeForever(
                                // Count down both latches when data is changed.
                                (value) -> {
                                    initializeCountdownLatch.countDown();
                                    changeCountdownLatch.countDown();
                                }));

        // Wait for the data to be read on the background thread.
        initializeCountdownLatch.await(5, TimeUnit.SECONDS);

        // Signal that the content has changed.
        mTestContentProvider.mTestEventCursor.signalDataChanged();

        // Wait for the changed data to be read on the background thread.
        changeCountdownLatch.await(5, TimeUnit.SECONDS);
    }

    private void runOnMain(Runnable runnable) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(runnable);
    }

    @Test
    public void addObserver_updateScheduled() throws InterruptedException {
        mTestHandler.setExpectedMessageCount(2);

        // Must add observer on main thread.
        runOnMain(
                () ->
                        mEventsLiveData.observeForever(
                                (value) -> {
                                    /* Do nothing */
                                }));

        mTestHandler.awaitExpectedMessages(5);

        // Show that a message was scheduled for the future.
        assertThat(mTestHandler.mLastUptimeMillis).isAtLeast(SystemClock.uptimeMillis());
    }

    @Test
    public void noCalendars_valueNull() throws InterruptedException {
        mTestContentProvider.mAddFakeCalendar = false;

        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);
        runOnMain(() -> mEventsLiveData.observeForever((value) -> latch.countDown()));

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        assertThat(mEventsLiveData.getValue()).isNull();
    }

    @Test
    @UiThreadTest
    public void noCalendars_contentObserved() throws InterruptedException {
        mTestContentProvider.mAddFakeCalendar = false;

        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);
        mEventsLiveData.observeForever((value) -> latch.countDown());

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        assertThat(mTestContentProvider.mTestEventCursor.mLastContentObserver).isNotNull();
    }

    @Test
    public void multiDayEvent_createsMultipleEvents() throws InterruptedException {
        // Replace the default event with one that lasts 24 hours.
        mTestContentProvider.addRow(buildTestRowWithDuration(CURRENT_DATE_TIME, 24));

        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);

        runOnMain(() -> mEventsLiveData.observeForever((value) -> latch.countDown()));

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        // Expect an event for the 2 parts of the split event instance.
        assertThat(mEventsLiveData.getValue()).hasSize(2);
    }

    @Test
    public void multiDayEvent_keepsOriginalTimes() throws InterruptedException {
        // Replace the default event with one that lasts 24 hours.
        int hours = 48;
        mTestContentProvider.addRow(buildTestRowWithDuration(CURRENT_DATE_TIME, hours));

        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);

        runOnMain(() -> mEventsLiveData.observeForever((value) -> latch.countDown()));

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        Event middlePartEvent = mEventsLiveData.getValue().get(1);

        // The start and end times should remain the original times.
        ZonedDateTime expectedStartTime = CURRENT_DATE_TIME.truncatedTo(HOURS);
        assertThat(middlePartEvent.getStartInstant()).isEqualTo(expectedStartTime.toInstant());
        ZonedDateTime expectedEndTime = expectedStartTime.plus(hours, HOURS);
        assertThat(middlePartEvent.getEndInstant()).isEqualTo(expectedEndTime.toInstant());
    }

    @Test
    public void multipleEvents_resultsSortedStart() throws InterruptedException {
        // Replace the default event with two that are out of time order.
        ZonedDateTime twoHoursAfterCurrentTime = CURRENT_DATE_TIME.plus(Duration.ofHours(2));
        mTestContentProvider.addRow(buildTestRowWithDuration(twoHoursAfterCurrentTime, 1));
        mTestContentProvider.addRow(buildTestRowWithDuration(CURRENT_DATE_TIME, 1));

        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);

        runOnMain(() -> mEventsLiveData.observeForever((value) -> latch.countDown()));

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        ImmutableList<Event> events = mEventsLiveData.getValue();

        assertThat(events.get(0).getStartInstant().toEpochMilli())
                .isEqualTo(addHoursAndTruncate(CURRENT_DATE_TIME, 0));
        assertThat(events.get(1).getStartInstant().toEpochMilli())
                .isEqualTo(addHoursAndTruncate(CURRENT_DATE_TIME, 2));
    }

    @Test
    public void multipleEvents_resultsSortedTitle() throws InterruptedException {
        // Replace the default event with two that are out of time order.
        mTestContentProvider.addRow(buildTestRowWithTitle(CURRENT_DATE_TIME, "Title B"));
        mTestContentProvider.addRow(buildTestRowWithTitle(CURRENT_DATE_TIME, "Title A"));
        mTestContentProvider.addRow(buildTestRowWithTitle(CURRENT_DATE_TIME, "Title C"));

        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);

        runOnMain(() -> mEventsLiveData.observeForever((value) -> latch.countDown()));

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        ImmutableList<Event> events = mEventsLiveData.getValue();

        assertThat(events.get(0).getTitle()).isEqualTo("Title A");
        assertThat(events.get(1).getTitle()).isEqualTo("Title B");
        assertThat(events.get(2).getTitle()).isEqualTo("Title C");
    }

    @Test
    public void allDayEvent_timesSetToLocal() throws InterruptedException {
        // All-day events always start at UTC midnight.
        ZonedDateTime utcMidnightStart =
                CURRENT_DATE_TIME.withZoneSameLocal(ZoneId.of("UTC")).truncatedTo(ChronoUnit.DAYS);
        mTestContentProvider.addRow(buildTestRowAllDay(utcMidnightStart));

        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);

        runOnMain(() -> mEventsLiveData.observeForever((value) -> latch.countDown()));

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        ImmutableList<Event> events = mEventsLiveData.getValue();

        Instant localMidnightStart = CURRENT_DATE_TIME.truncatedTo(ChronoUnit.DAYS).toInstant();
        assertThat(events.get(0).getStartInstant()).isEqualTo(localMidnightStart);
    }

    @Test
    public void allDayEvent_queryCoversLocalDayStart() throws InterruptedException {
        // All-day events always start at UTC midnight.
        ZonedDateTime utcMidnightStart =
                CURRENT_DATE_TIME.withZoneSameLocal(ZoneId.of("UTC")).truncatedTo(ChronoUnit.DAYS);
        mTestContentProvider.addRow(buildTestRowAllDay(utcMidnightStart));

        // Set the time to 23:XX in the BERLIN_ZONE_ID which will be after the event end time.
        mTestClock.setTime(CURRENT_DATE_TIME.with(ChronoField.HOUR_OF_DAY, 23));

        // Expect onChanged to be called for when we start to observe and when the data is read.
        CountDownLatch latch = new CountDownLatch(2);

        runOnMain(() -> mEventsLiveData.observeForever((value) -> latch.countDown()));

        // Wait for the data to be read on the background thread.
        latch.await(5, TimeUnit.SECONDS);

        // Show that the event is included even though its end time is before the current time.
        assertThat(mEventsLiveData.getValue()).isNotEmpty();
    }

    private static class TestContentProvider extends MockContentProvider {
        TestEventCursor mTestEventCursor;
        boolean mAddFakeCalendar = true;
        List<Object[]> mEventRows = new ArrayList<>();

        TestContentProvider(Context context) {
            super(context);
        }

        private void addRow(Object[] row) {
            mEventRows.add(row);
        }

        @Override
        public Cursor query(
                Uri uri,
                String[] projection,
                Bundle queryArgs,
                CancellationSignal cancellationSignal) {
            if (uri.toString().startsWith(CalendarContract.Instances.CONTENT_URI.toString())) {
                mTestEventCursor = new TestEventCursor(uri);
                for (Object[] row : mEventRows) {
                    mTestEventCursor.addRow(row);
                }
                return mTestEventCursor;
            } else if (uri.equals(CalendarContract.Calendars.CONTENT_URI)) {
                MatrixCursor calendarsCursor = new MatrixCursor(new String[] {" Test name"});
                if (mAddFakeCalendar) {
                    calendarsCursor.addRow(new String[] {"Test value"});
                }
                return calendarsCursor;
            }
            throw new IllegalStateException("Unexpected query uri " + uri);
        }

        static class TestEventCursor extends MatrixCursor {
            final Uri mUri;
            ContentObserver mLastContentObserver;

            TestEventCursor(Uri uri) {
                super(
                        new String[] {
                            CalendarContract.Instances.TITLE,
                            CalendarContract.Instances.ALL_DAY,
                            CalendarContract.Instances.BEGIN,
                            CalendarContract.Instances.END,
                            CalendarContract.Instances.DESCRIPTION,
                            CalendarContract.Instances.EVENT_LOCATION,
                            CalendarContract.Instances.SELF_ATTENDEE_STATUS,
                            CalendarContract.Instances.CALENDAR_COLOR,
                            CalendarContract.Instances.CALENDAR_DISPLAY_NAME,
                        });
                mUri = uri;
            }

            @Override
            public void registerContentObserver(ContentObserver observer) {
                super.registerContentObserver(observer);
                mLastContentObserver = observer;
            }

            @Override
            public void unregisterContentObserver(ContentObserver observer) {
                super.unregisterContentObserver(observer);
                mLastContentObserver = null;
            }

            void signalDataChanged() {
                super.onChange(true);
            }
        }
    }

    private static class TestHandler extends Handler {
        final HandlerThread mThread;
        long mLastUptimeMillis;
        CountDownLatch mCountDownLatch;

        static TestHandler create() {
            HandlerThread thread =
                    new HandlerThread(
                            EventsLiveDataTest.class.getSimpleName(),
                            Process.THREAD_PRIORITY_FOREGROUND);
            thread.start();
            return new TestHandler(thread);
        }

        TestHandler(HandlerThread thread) {
            super(thread.getLooper());
            mThread = thread;
        }

        void stop() {
            mThread.quit();
        }

        void setExpectedMessageCount(int expectedMessageCount) {
            mCountDownLatch = new CountDownLatch(expectedMessageCount);
        }

        void awaitExpectedMessages(int seconds) throws InterruptedException {
            mCountDownLatch.await(seconds, TimeUnit.SECONDS);
        }

        @Override
        public boolean sendMessageAtTime(Message msg, long uptimeMillis) {
            mLastUptimeMillis = uptimeMillis;
            if (mCountDownLatch != null) {
                mCountDownLatch.countDown();
            }
            return super.sendMessageAtTime(msg, uptimeMillis);
        }
    }

    // Similar to {@link android.os.SimpleClock} but without @hide and with mutable millis.
    static class TestClock extends Clock {
        private final ZoneId mZone;
        private long mTimeMs;

        TestClock(ZoneId zone) {
            mZone = zone;
        }

        void setTime(ZonedDateTime time) {
            mTimeMs = time.toInstant().toEpochMilli();
        }

        @Override
        public ZoneId getZone() {
            return mZone;
        }

        @Override
        public Clock withZone(ZoneId zone) {
            return new TestClock(zone) {
                @Override
                public long millis() {
                    return TestClock.this.millis();
                }
            };
        }

        @Override
        public long millis() {
            return mTimeMs;
        }

        @Override
        public Instant instant() {
            return Instant.ofEpochMilli(millis());
        }
    }

    static long addHoursAndTruncate(ZonedDateTime dateTime, int hours) {
        return dateTime.truncatedTo(HOURS)
                .plus(Duration.ofHours(hours))
                .toInstant()
                .toEpochMilli();
    }

    static Object[] buildTestRowWithDuration(ZonedDateTime startDateTime, int eventDurationHours) {
        return buildTestRowWithDuration(
                startDateTime, eventDurationHours, EVENT_TITLE, EVENT_ALL_DAY);
    }

    static Object[] buildTestRowAllDay(ZonedDateTime startDateTime) {
        return buildTestRowWithDuration(startDateTime, 24, EVENT_TITLE, true);
    }

    static Object[] buildTestRowWithTitle(ZonedDateTime startDateTime, String title) {
        return buildTestRowWithDuration(startDateTime, 1, title, EVENT_ALL_DAY);
    }

    static Object[] buildTestRowWithDuration(
            ZonedDateTime currentDateTime, int eventDurationHours, String title, boolean allDay) {
        return new Object[] {
            title,
            allDay ? 1 : 0,
            addHoursAndTruncate(currentDateTime, 0),
            addHoursAndTruncate(currentDateTime, eventDurationHours),
            EVENT_DESCRIPTION,
            EVENT_LOCATION,
            EVENT_ATTENDEE_STATUS,
            CALENDAR_COLOR,
            CALENDAR_NAME
        };
    }
}
