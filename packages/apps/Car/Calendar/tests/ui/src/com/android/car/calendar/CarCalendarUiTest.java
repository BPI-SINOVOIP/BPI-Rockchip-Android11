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

package com.android.car.calendar;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.provider.CalendarContract;
import android.telephony.TelephonyManager;
import android.test.mock.MockContentProvider;
import android.test.mock.MockContentResolver;

import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProvider;
import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.GrantPermissionRule;
import androidx.test.runner.lifecycle.ActivityLifecycleCallback;
import androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry;
import androidx.test.runner.lifecycle.Stage;

import com.android.car.calendar.common.Event;
import com.android.car.calendar.common.EventsLiveData;

import com.google.common.collect.ImmutableList;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.time.Clock;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.time.temporal.ChronoUnit;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class CarCalendarUiTest {
    private static final ZoneId BERLIN_ZONE_ID = ZoneId.of("Europe/Berlin");
    private static final ZoneId UTC_ZONE_ID = ZoneId.of("UTC");
    private static final Locale LOCALE = Locale.ENGLISH;
    private static final ZonedDateTime CURRENT_DATE_TIME =
            LocalDateTime.of(2019, 12, 10, 10, 10, 10, 500500).atZone(BERLIN_ZONE_ID);
    private static final ZonedDateTime START_DATE_TIME =
            CURRENT_DATE_TIME.truncatedTo(ChronoUnit.HOURS);
    private static final String EVENT_TITLE = "the title";
    private static final String EVENT_LOCATION = "the location";
    private static final String EVENT_DESCRIPTION = "the description";
    private static final String CALENDAR_NAME = "the calendar name";
    private static final int CALENDAR_COLOR = 0xCAFEBABE;
    private static final int EVENT_ATTENDEE_STATUS =
            CalendarContract.Attendees.ATTENDEE_STATUS_ACCEPTED;

    private final ActivityLifecycleCallback mLifecycleCallback = this::onActivityLifecycleChanged;

    @Rule
    public final GrantPermissionRule permissionRule =
            GrantPermissionRule.grant(Manifest.permission.READ_CALENDAR);

    private List<Object[]> mTestEventRows;

    // These can be set in the test thread and read on the main thread.
    private volatile CountDownLatch mEventChangesLatch;

    @Before
    public void setUp() {
        ActivityLifecycleMonitorRegistry.getInstance().addLifecycleCallback(mLifecycleCallback);
        mTestEventRows = new ArrayList<>();
    }

    private void onActivityLifecycleChanged(Activity activity, Stage stage) {
        if (stage.equals(Stage.PRE_ON_CREATE)) {
            setActivityDependencies((CarCalendarActivity) activity);
        } else if (stage.equals(Stage.CREATED)) {
            observeEventsLiveData((CarCalendarActivity) activity);
        }
    }

    private void setActivityDependencies(CarCalendarActivity activity) {
        Clock fixedTimeClock = Clock.fixed(CURRENT_DATE_TIME.toInstant(), BERLIN_ZONE_ID);
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        MockContentResolver mockContentResolver = new MockContentResolver(context);
        TestCalendarContentProvider testCalendarContentProvider =
                new TestCalendarContentProvider(context);
        mockContentResolver.addProvider(CalendarContract.AUTHORITY, testCalendarContentProvider);
        activity.mDependencies =
                new CarCalendarActivity.Dependencies(LOCALE, fixedTimeClock, mockContentResolver,
                        activity.getSystemService(TelephonyManager.class));
    }

    private void observeEventsLiveData(CarCalendarActivity activity) {
        CarCalendarViewModel carCalendarViewModel =
                new ViewModelProvider(activity).get(CarCalendarViewModel.class);
        EventsLiveData eventsLiveData = carCalendarViewModel.getEventsLiveData();
        mEventChangesLatch = new CountDownLatch(1);

        // Notifications occur on the main thread.
        eventsLiveData.observeForever(
                new Observer<ImmutableList<Event>>() {
                    // Ignore the first change event triggered on registration with default value.
                    boolean mIgnoredFirstChange;

                    @Override
                    public void onChanged(ImmutableList<Event> events) {
                        if (mIgnoredFirstChange) {
                            // Signal that the events were changed and notified on main thread.
                            mEventChangesLatch.countDown();
                        }
                        mIgnoredFirstChange = true;
                    }
                });
    }

    @After
    public void tearDown() {
        ActivityLifecycleMonitorRegistry.getInstance().removeLifecycleCallback(mLifecycleCallback);
    }

    @Test
    public void calendar_titleShows() {
        try (ActivityScenario<CarCalendarActivity> ignored =
                ActivityScenario.launch(CarCalendarActivity.class)) {
            onView(withText(R.string.app_name)).check(matches(isDisplayed()));
        }
    }

    @Test
    public void event_displayed() {
        mTestEventRows.add(buildTestRow(START_DATE_TIME, 1, EVENT_TITLE, false));
        try (ActivityScenario<CarCalendarActivity> ignored =
                ActivityScenario.launch(CarCalendarActivity.class)) {
            waitForEventsChange();

            // Wait for the UI to be updated with changed events.
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();

            onView(withText(EVENT_TITLE)).check(matches(isDisplayed()));
        }
    }

    @Test
    public void singleAllDayEvent_notCollapsed() {
        // All day events are stored in UTC time.
        ZonedDateTime utcDayStartTime =
                START_DATE_TIME.withZoneSameInstant(UTC_ZONE_ID).truncatedTo(ChronoUnit.DAYS);

        mTestEventRows.add(buildTestRow(utcDayStartTime, 24, EVENT_TITLE, true));

        try (ActivityScenario<CarCalendarActivity> ignored =
                ActivityScenario.launch(CarCalendarActivity.class)) {
            waitForEventsChange();

            // Wait for the UI to be updated with changed events.
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();

            // A single all-day event should not be collapsible.
            onView(withId(R.id.expand_collapse_icon)).check(doesNotExist());
            onView(withText(EVENT_TITLE)).check(matches(isDisplayed()));
        }
    }

    @Test
    public void multipleAllDayEvents_collapsed() {
        mTestEventRows.add(buildTestRowAllDay(EVENT_TITLE));
        mTestEventRows.add(buildTestRowAllDay("Another all day event"));

        try (ActivityScenario<CarCalendarActivity> ignored =
                ActivityScenario.launch(CarCalendarActivity.class)) {
            waitForEventsChange();

            // Wait for the UI to be updated with changed events.
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();

            // Multiple all-day events should be collapsed.
            onView(withId(R.id.expand_collapse_icon)).check(matches(isDisplayed()));
            onView(withText(EVENT_TITLE)).check(matches(not(isDisplayed())));
        }
    }

    @Test
    public void multipleAllDayEvents_expands() {
        mTestEventRows.add(buildTestRowAllDay(EVENT_TITLE));
        mTestEventRows.add(buildTestRowAllDay("Another all day event"));

        try (ActivityScenario<CarCalendarActivity> ignored =
                ActivityScenario.launch(CarCalendarActivity.class)) {
            waitForEventsChange();

            // Wait for the UI to be updated with changed events.
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();

            // Multiple all-day events should be collapsed.
            onView(withId(R.id.expand_collapse_icon)).perform(click());
            InstrumentationRegistry.getInstrumentation().waitForIdleSync();
            onView(withText(EVENT_TITLE)).check(matches(isDisplayed()));
        }
    }

    private void waitForEventsChange() {
        try {
            mEventChangesLatch.await(10, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    private class TestCalendarContentProvider extends MockContentProvider {
        TestCalendarContentProvider(Context context) {
            super(context);
        }

        @Override
        public Cursor query(
                Uri uri,
                String[] projection,
                Bundle queryArgs,
                CancellationSignal cancellationSignal) {
            if (uri.toString().startsWith(CalendarContract.Instances.CONTENT_URI.toString())) {
                MatrixCursor cursor =
                        new MatrixCursor(
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
                for (Object[] row : mTestEventRows) {
                    cursor.addRow(row);
                }
                return cursor;
            } else if (uri.equals(CalendarContract.Calendars.CONTENT_URI)) {
                MatrixCursor cursor = new MatrixCursor(new String[] {" Test name"});
                cursor.addRow(new String[] {"Test value"});
                return cursor;
            }
            throw new IllegalStateException("Unexpected query uri " + uri);
        }
    }

    private Object[] buildTestRowAllDay(String title) {
        // All day events are stored in UTC time.
        ZonedDateTime utcDayStartTime =
                START_DATE_TIME.withZoneSameInstant(UTC_ZONE_ID).truncatedTo(ChronoUnit.DAYS);
        return buildTestRow(utcDayStartTime, 24, title, true);
    }

    private static Object[] buildTestRow(
            ZonedDateTime startDateTime, int eventDurationHours, String title, boolean allDay) {
        return new Object[] {
            title,
            allDay ? 1 : 0,
            startDateTime.toInstant().toEpochMilli(),
            startDateTime.plusHours(eventDurationHours).toInstant().toEpochMilli(),
            EVENT_DESCRIPTION,
            EVENT_LOCATION,
            EVENT_ATTENDEE_STATUS,
            CALENDAR_COLOR,
            CALENDAR_NAME
        };
    }
}
