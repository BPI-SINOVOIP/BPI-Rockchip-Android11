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

import static com.google.common.base.Preconditions.checkNotNull;

import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import java.util.List;

class AllDayEventsItem implements CalendarItem {

    private final List<EventCalendarItem> mAllDayEventItems;

    AllDayEventsItem(List<EventCalendarItem> allDayEventItems) {
        mAllDayEventItems = allDayEventItems;
    }

    @Override
    public CalendarItem.Type getType() {
        return Type.ALL_DAY_EVENTS;
    }

    @Override
    public void bind(RecyclerView.ViewHolder holder) {
        ((AllDayEventsViewHolder) holder).update(mAllDayEventItems);
    }

    static class AllDayEventsViewHolder extends RecyclerView.ViewHolder {

        private final ImageView mExpandCollapseIcon;
        private final TextView mTitleTextView;
        private final Resources mResources;
        private final LinearLayout mEventItemsView;
        private boolean mExpanded;

        AllDayEventsViewHolder(ViewGroup parent) {
            super(
                    LayoutInflater.from(parent.getContext())
                            .inflate(
                                    R.layout.all_day_events_item,
                                    parent,
                                    /* attachToRoot= */ false));
            mExpandCollapseIcon = checkNotNull(itemView.findViewById(R.id.expand_collapse_icon));
            mTitleTextView = checkNotNull(itemView.findViewById(R.id.title_text));
            mEventItemsView = checkNotNull(itemView.findViewById(R.id.events));

            mResources = parent.getResources();
            View expandCollapseView = checkNotNull(itemView.findViewById(R.id.expand_collapse));
            expandCollapseView.setOnClickListener(this::onToggleClick);
        }

        void update(List<EventCalendarItem> eventCalendarItems) {
            mEventItemsView.removeAllViews();
            mExpanded = false;
            hideEventSection();

            int size = eventCalendarItems.size();
            mTitleTextView.setText(
                    mResources.getQuantityString(R.plurals.all_day_title, size, size));

            for (EventCalendarItem eventCalendarItem : eventCalendarItems) {
                EventCalendarItem.EventViewHolder holder =
                        new EventCalendarItem.EventViewHolder(mEventItemsView);
                mEventItemsView.addView(holder.itemView);
                eventCalendarItem.bind(holder);
            }
        }

        private void onToggleClick(View view) {
            mExpanded = !mExpanded;
            if (mExpanded) {
                showEventSection();
            } else {
                hideEventSection();
            }
        }

        private void hideEventSection() {
            mExpandCollapseIcon.setImageResource(R.drawable.ic_navigation_expand_more_white_24dp);
            mEventItemsView.setVisibility(View.GONE);
        }

        private void showEventSection() {
            mExpandCollapseIcon.setImageResource(R.drawable.ic_navigation_expand_less_white_24dp);
            mEventItemsView.setVisibility(View.VISIBLE);
        }
    }
}
