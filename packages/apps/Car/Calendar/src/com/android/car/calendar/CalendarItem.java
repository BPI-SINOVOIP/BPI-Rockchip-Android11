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

import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;

/**
 * Represents an item that can be displayed in the calendar list. It will hold the data needed to
 * populate the {@link androidx.recyclerview.widget.RecyclerView.ViewHolder} passed in to the {@link
 * #bind(RecyclerView.ViewHolder)} method.
 */
interface CalendarItem {

    /** Returns the type of this calendar item instance. */
    Type getType();

    /** Bind the view holder with the data represented by this item. */
    void bind(RecyclerView.ViewHolder holder);

    /** The type of the calendar item which knows how to create a view holder for */
    enum Type {
        EVENT {
            @Override
            RecyclerView.ViewHolder createViewHolder(ViewGroup parent) {
                return new EventCalendarItem.EventViewHolder(parent);
            }
        },
        TITLE {
            @Override
            RecyclerView.ViewHolder createViewHolder(ViewGroup parent) {
                return new TitleCalendarItem.TitleViewHolder(parent);
            }
        },
        ALL_DAY_EVENTS {
            @Override
            RecyclerView.ViewHolder createViewHolder(ViewGroup parent) {
                return new AllDayEventsItem.AllDayEventsViewHolder(parent);
            }
        };

        /** Creates a view holder for this type of calendar item. */
        abstract RecyclerView.ViewHolder createViewHolder(ViewGroup parent);
    }
}
