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

import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import com.android.car.calendar.common.CalendarFormatter;

import java.time.LocalDate;

/** An item in the calendar list view that shows a title row for the following list of events. */
class TitleCalendarItem implements CalendarItem {

    private final LocalDate mDate;
    private final CalendarFormatter mFormatter;

    TitleCalendarItem(LocalDate date, CalendarFormatter formatter) {
        mDate = date;
        mFormatter = formatter;
    }

    @Override
    public Type getType() {
        return Type.TITLE;
    }

    @Override
    public void bind(RecyclerView.ViewHolder holder) {
        TitleViewHolder titleViewHolder = (TitleViewHolder) holder;
        titleViewHolder.update(mFormatter.getDateText(mDate));
    }

    static class TitleViewHolder extends RecyclerView.ViewHolder {
        private final TextView mDateTextView;

        TitleViewHolder(ViewGroup parent) {
            super(
                    LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.title_item, parent, /* attachToRoot= */ false));
            mDateTextView = itemView.findViewById(R.id.date_text);
        }

        void update(String dateText) {
            mDateTextView.setText(dateText);
        }
    }
}
