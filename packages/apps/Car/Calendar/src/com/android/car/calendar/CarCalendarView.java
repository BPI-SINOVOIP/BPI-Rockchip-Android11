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

import static com.google.common.base.Verify.verify;
import static com.google.common.base.Verify.verifyNotNull;

import android.Manifest;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.Observer;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.calendar.common.CalendarFormatter;
import com.android.car.calendar.common.Dialer;
import com.android.car.calendar.common.Event;
import com.android.car.calendar.common.EventsLiveData;
import com.android.car.calendar.common.Navigator;
import com.android.car.ui.recyclerview.CarUiRecyclerView;

import com.google.common.collect.ImmutableList;

import java.time.Duration;
import java.time.LocalDate;
import java.time.ZoneId;
import java.util.ArrayList;
import java.util.List;

/** The main calendar app view. */
class CarCalendarView {
    private static final String TAG = "CarCalendarView";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    /** Activity referenced as concrete type to access permissions methods. */
    private final CarCalendarActivity mCarCalendarActivity;

    /** The main calendar view. */
    private final CarCalendarViewModel mCarCalendarViewModel;

    private final Navigator mNavigator;
    private final Dialer mDialer;
    private final CalendarFormatter mFormatter;
    private final TextView mNoEventsTextView;

    /** Holds an instance of either {@link LocalDate} or {@link Event} for each item in the list. */
    private final List<CalendarItem> mRecyclerViewItems = new ArrayList<>();

    private final RecyclerView.Adapter mAdapter = new EventRecyclerViewAdapter();
    private final Observer<ImmutableList<Event>> mEventsObserver =
            events -> {
                if (DEBUG) Log.d(TAG, "Events changed");
                updateRecyclerViewItems(events);

                // TODO(jdp) Only change the affected items (DiffUtil) to allow animated changes.
                mAdapter.notifyDataSetChanged();
            };

    CarCalendarView(
            CarCalendarActivity carCalendarActivity,
            CarCalendarViewModel carCalendarViewModel,
            Navigator navigator,
            Dialer dialer,
            CalendarFormatter formatter) {
        mCarCalendarActivity = carCalendarActivity;
        mCarCalendarViewModel = carCalendarViewModel;
        mNavigator = navigator;
        mDialer = dialer;
        mFormatter = formatter;

        carCalendarActivity.setContentView(R.layout.calendar);
        CarUiRecyclerView calendarRecyclerView = carCalendarActivity.findViewById(R.id.events);
        mNoEventsTextView = carCalendarActivity.findViewById(R.id.no_events_text);
        calendarRecyclerView.setHasFixedSize(true);
        calendarRecyclerView.setAdapter(mAdapter);
    }

    void show() {
        // TODO(jdp) If permission is denied then show some UI to allow them to retry.
        mCarCalendarActivity.runWithPermission(
                Manifest.permission.READ_CALENDAR, this::showWithPermission);
    }

    private void showWithPermission() {
        EventsLiveData eventsLiveData = mCarCalendarViewModel.getEventsLiveData();
        eventsLiveData.observe(mCarCalendarActivity, mEventsObserver);
        updateRecyclerViewItems(verifyNotNull(eventsLiveData.getValue()));
    }

    /**
     * If the events list is null there is no calendar data available. If the events list is empty
     * there is calendar data but no events.
     */
    private void updateRecyclerViewItems(@Nullable ImmutableList<Event> carCalendarEvents) {
        LocalDate currentDate = null;
        mRecyclerViewItems.clear();

        if (carCalendarEvents == null) {
            mNoEventsTextView.setVisibility(View.VISIBLE);
            mNoEventsTextView.setText(R.string.no_calendars);
            return;
        }
        if (carCalendarEvents.isEmpty()) {
            mNoEventsTextView.setVisibility(View.VISIBLE);
            mNoEventsTextView.setText(R.string.no_events);
            return;
        }
        mNoEventsTextView.setVisibility(View.GONE);

        // Add all rows in the calendar list.
        // A day might have all-day events that need to be added before regular events so we need to
        // add the event rows after looking at all events for the day.
        List<CalendarItem> eventItems = null;
        List<EventCalendarItem> allDayEventItems = null;
        for (Event event : carCalendarEvents) {
            LocalDate date =
                    event.getDayStartInstant().atZone(ZoneId.systemDefault()).toLocalDate();

            // Start a new section when the date changes.
            if (!date.equals(currentDate)) {
                verify(
                        currentDate == null || !date.isBefore(currentDate),
                        "Expected events to be sorted by start time");
                currentDate = date;

                // Add the events from the previous day.
                if (eventItems != null) {
                    verify(allDayEventItems != null);
                    addAllEvents(allDayEventItems, eventItems);
                }

                mRecyclerViewItems.add(new TitleCalendarItem(date, mFormatter));
                allDayEventItems = new ArrayList<>();
                eventItems = new ArrayList<>();
            }

            // Events that last 24 hours or longer are also shown with all day events.
            if (event.isAllDay() || event.getDuration().compareTo(Duration.ofDays(1)) >= 0) {
                // Only add a row when necessary because hiding it can leave padding or decorations.
                allDayEventItems.add(
                        new EventCalendarItem(
                                event, mFormatter, mNavigator, mDialer, mCarCalendarActivity));
            } else {
                eventItems.add(
                        new EventCalendarItem(
                                event, mFormatter, mNavigator, mDialer, mCarCalendarActivity));
            }
        }
        addAllEvents(allDayEventItems, eventItems);
    }

    private void addAllEvents(
            List<EventCalendarItem> allDayEventItems, List<CalendarItem> eventItems) {
        if (allDayEventItems.size() > 1) {
            mRecyclerViewItems.add(new AllDayEventsItem(allDayEventItems));
        } else if (allDayEventItems.size() == 1) {
            mRecyclerViewItems.add(allDayEventItems.get(0));
        }
        mRecyclerViewItems.addAll(eventItems);
    }

    private class EventRecyclerViewAdapter extends RecyclerView.Adapter {

        @NonNull
        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            CalendarItem.Type type = CalendarItem.Type.values()[viewType];
            return type.createViewHolder(parent);
        }

        @Override
        public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
            mRecyclerViewItems.get(position).bind(holder);
        }

        @Override
        public int getItemCount() {
            return mRecyclerViewItems.size();
        }

        @Override
        public int getItemViewType(int position) {
            return mRecyclerViewItems.get(position).getType().ordinal();
        }
    }
}
