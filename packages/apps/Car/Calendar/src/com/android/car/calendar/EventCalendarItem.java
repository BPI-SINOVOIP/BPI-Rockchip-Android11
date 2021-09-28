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

import android.Manifest;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.ShapeDrawable;
import android.graphics.drawable.shapes.OvalShape;
import android.text.SpannableString;
import android.text.style.ForegroundColorSpan;
import android.text.style.ImageSpan;
import android.text.style.StyleSpan;
import android.view.LayoutInflater;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.calendar.common.CalendarFormatter;
import com.android.car.calendar.common.Dialer;
import com.android.car.calendar.common.Event;
import com.android.car.calendar.common.Navigator;

import com.google.common.base.Joiner;
import com.google.common.base.Strings;
import com.google.common.collect.Lists;

import javax.annotation.Nullable;

/** An item in the calendar list view that shows a single event. */
class EventCalendarItem implements CalendarItem {
    private final Event mEvent;
    private final CalendarFormatter mFormatter;
    private final Navigator mNavigator;
    private final Dialer mDialer;
    private final CarCalendarActivity mCarCalendarActivity;

    EventCalendarItem(
            Event event,
            CalendarFormatter formatter,
            Navigator navigator,
            Dialer dialer,
            CarCalendarActivity carCalendarActivity) {
        mEvent = event;
        mFormatter = formatter;
        mNavigator = navigator;
        mDialer = dialer;
        mCarCalendarActivity = carCalendarActivity;
    }

    @Override
    public Type getType() {
        return Type.EVENT;
    }

    @Override
    public void bind(RecyclerView.ViewHolder holder) {
        EventViewHolder eventViewHolder = (EventViewHolder) holder;

        EventAction primaryAction;
        if (!Strings.isNullOrEmpty(mEvent.getLocation())) {
            primaryAction =
                    new EventAction(
                            R.drawable.ic_navigation_gm2_24px,
                            mEvent.getLocation(),
                            (view) -> mNavigator.navigate(mEvent.getLocation()));
        } else {
            primaryAction =
                    new EventAction(
                            R.drawable.ic_navigation_gm2_24px, /* descriptionText */
                            null, /* onClickHandler */
                            null);
        }

        EventAction secondaryAction;
        Dialer.NumberAndAccess numberAndAccess = mEvent.getNumberAndAccess();
        if (numberAndAccess != null) {
            String dialDescriptionText;
            if (numberAndAccess.getAccess() != null) {
                dialDescriptionText =
                        mCarCalendarActivity.getString(
                                R.string.phone_number_with_pin,
                                numberAndAccess.getNumber(),
                                numberAndAccess.getAccess());

            } else {
                dialDescriptionText =
                        mCarCalendarActivity.getString(
                                R.string.phone_number, numberAndAccess.getNumber());
            }
            secondaryAction =
                    new EventAction(
                            R.drawable.ic_phone_gm2_24px,
                            dialDescriptionText,
                            (view) -> dial(numberAndAccess));
        } else {
            secondaryAction =
                    new EventAction(
                            R.drawable.ic_phone_gm2_24px,
                            /* descriptionText */ null,
                            /* onClickListener= */ null);
        }

        String timeRangeText;
        if (!mEvent.isAllDay()) {
            timeRangeText =
                    mFormatter.getTimeRangeText(mEvent.getStartInstant(), mEvent.getEndInstant());
        } else {
            timeRangeText = mCarCalendarActivity.getString(R.string.all_day_event);
        }
        eventViewHolder.update(
                timeRangeText,
                mEvent.getTitle(),
                mEvent.getCalendarDetails().getColor(),
                mEvent.getCalendarDetails().getName(),
                primaryAction,
                secondaryAction,
                mEvent.getStatus());
    }

    private void dial(Dialer.NumberAndAccess numberAndAccess) {
        mCarCalendarActivity.runWithPermission(
                Manifest.permission.CALL_PHONE,
                () -> {
                    if (!mDialer.dial(numberAndAccess)) {
                        Toast.makeText(mCarCalendarActivity, R.string.no_dialler, Toast.LENGTH_LONG)
                                .show();
                    }
                });
    }

    private static class EventAction {
        @DrawableRes private final int mIconResourceId;
        @Nullable private final String mDescriptionText;
        @Nullable private final OnClickListener mOnClickListener;

        private EventAction(
                int iconResourceId,
                @Nullable String descriptionText,
                @Nullable OnClickListener onClickListener) {
            this.mIconResourceId = iconResourceId;
            this.mDescriptionText = descriptionText;
            this.mOnClickListener = onClickListener;
        }
    }

    static class EventViewHolder extends RecyclerView.ViewHolder {
        private static final String FIELD_SEPARATOR = ", ";
        private static final Joiner JOINER = Joiner.on(FIELD_SEPARATOR).skipNulls();

        private final TextView mTitleView;
        private final TextView mDescriptionView;
        private final ImageButton mPrimaryActionButton;
        private final ImageButton mSecondaryActionButton;
        private final int mCalendarIndicatorSize;
        private final int mCalendarIndicatorPadding;
        @ColorInt private final int mTimeTextColor;

        EventViewHolder(ViewGroup parent) {
            super(
                    LayoutInflater.from(parent.getContext())
                            .inflate(R.layout.event_item, parent, /* attachToRoot= */ false));
            mTitleView = itemView.findViewById(R.id.event_title);
            mDescriptionView = itemView.findViewById(R.id.description_text);
            mPrimaryActionButton = itemView.findViewById(R.id.primary_action_button);
            mSecondaryActionButton = itemView.findViewById(R.id.secondary_action_button);
            mCalendarIndicatorSize =
                    (int) parent.getResources().getDimension(R.dimen.car_calendar_indicator_width);
            mCalendarIndicatorPadding =
                    (int) parent.getResources().getDimension(R.dimen.car_ui_padding_1);
            mTimeTextColor =
                    ContextCompat.getColor(parent.getContext(), R.color.car_ui_text_color_primary);
        }

        void update(
                String timeRangeText,
                String title,
                @ColorInt int calendarColor,
                String calendarName,
                EventAction primaryAction,
                EventAction secondaryAction,
                Event.Status status) {

            mTitleView.setText(title);

            String detailText = null;
            if (primaryAction.mDescriptionText != null) {
                detailText = primaryAction.mDescriptionText;
            } else if (secondaryAction.mDescriptionText != null) {
                detailText = secondaryAction.mDescriptionText;
            }
            SpannableString descriptionSpannable =
                    createDescriptionSpannable(
                            calendarColor, calendarName, timeRangeText, detailText);

            mDescriptionView.setText(descriptionSpannable);

            // Strike-through all text fields when the event was declined.
            setTextFlags(
                    Paint.STRIKE_THRU_TEXT_FLAG,
                    /* add= */ status.equals(Event.Status.DECLINED),
                    mTitleView,
                    mDescriptionView);

            mPrimaryActionButton.setImageResource(primaryAction.mIconResourceId);
            if (primaryAction.mOnClickListener != null) {
                mPrimaryActionButton.setEnabled(true);
                mPrimaryActionButton.setContentDescription(primaryAction.mDescriptionText);
                mPrimaryActionButton.setOnClickListener(primaryAction.mOnClickListener);
            } else {
                mPrimaryActionButton.setEnabled(false);
                mPrimaryActionButton.setContentDescription(null);
                mPrimaryActionButton.setOnClickListener(null);
            }

            mSecondaryActionButton.setImageResource(secondaryAction.mIconResourceId);
            if (secondaryAction.mOnClickListener != null) {
                mSecondaryActionButton.setEnabled(true);
                mSecondaryActionButton.setContentDescription(secondaryAction.mDescriptionText);
                mSecondaryActionButton.setOnClickListener(secondaryAction.mOnClickListener);
            } else {
                mSecondaryActionButton.setEnabled(false);
                mSecondaryActionButton.setContentDescription(null);
                mSecondaryActionButton.setOnClickListener(null);
            }
        }

        private SpannableString createDescriptionSpannable(
                @ColorInt int calendarColor,
                String calendarName,
                String timeRangeText,
                @Nullable String detailText) {
            ShapeDrawable calendarIndicatorDrawable = new ShapeDrawable(new OvalShape());
            calendarIndicatorDrawable.getPaint().setColor(calendarColor);

            calendarIndicatorDrawable.setBounds(
                    /* left= */ 0,
                    /* top= */ 0,
                    /* right= */ mCalendarIndicatorSize,
                    /* bottom= */ mCalendarIndicatorSize);

            // Add padding to the right of the image to separate it from the text.
            InsetDrawable insetDrawable =
                    new InsetDrawable(
                            calendarIndicatorDrawable,
                            /* insetLeft= */ 0,
                            /* insetTop= */ 0,
                            /* insetRight= */ mCalendarIndicatorPadding,
                            /* insetBottom= */ 0);

            insetDrawable.setBounds(
                    /* left= */ 0,
                    /* top= */ 0,
                    /* right= */ mCalendarIndicatorSize + mCalendarIndicatorPadding,
                    /* bottom= */ mCalendarIndicatorSize);

            String descriptionText =
                    JOINER.join(Lists.newArrayList(calendarName, timeRangeText, detailText));
            SpannableString descriptionSpannable = new SpannableString(descriptionText);
            ImageSpan calendarIndicatorSpan =
                    new ImageSpan(insetDrawable, ImageSpan.ALIGN_BASELINE);
            int calendarNameEnd = calendarName.length() + FIELD_SEPARATOR.length();
            descriptionSpannable.setSpan(
                    calendarIndicatorSpan, /* start= */ 0, calendarNameEnd, /* flags= */ 0);
            int timeEnd = calendarNameEnd + timeRangeText.length();
            descriptionSpannable.setSpan(
                    new StyleSpan(Typeface.BOLD), calendarNameEnd, timeEnd, /* flags= */ 0);
            descriptionSpannable.setSpan(
                    new ForegroundColorSpan(mTimeTextColor),
                    calendarNameEnd,
                    timeEnd,
                    /* flags= */ 0);
            return descriptionSpannable;
        }

        /**
         * Set paint flags on the given text views.
         *
         * @param flags The combined {@link Paint} flags to set or unset.
         * @param set Set the flags if true, otherwise unset.
         * @param views The views to apply the flags to.
         */
        private void setTextFlags(int flags, boolean set, TextView... views) {
            for (TextView view : views) {
                if (set) {
                    view.setPaintFlags(view.getPaintFlags() | flags);
                } else {
                    view.setPaintFlags(view.getPaintFlags() & ~flags);
                }
            }
        }
    }
}
