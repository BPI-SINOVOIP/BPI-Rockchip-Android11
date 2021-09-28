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
package com.android.car.notification.template;

import static android.app.Notification.EXTRA_SUBSTITUTE_APP_NAME;

import android.annotation.ColorInt;
import android.app.Notification;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.service.notification.StatusBarNotification;
import android.text.BidiFormatter;
import android.text.TextDirectionHeuristics;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;
import android.view.View;
import android.widget.DateTimeView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;

import com.android.car.notification.AlertEntry;
import com.android.car.notification.R;

/**
 * Notification header view that contains the issuer app icon and name, and extra information.
 */
public class CarNotificationHeaderView extends LinearLayout {

    private static final String TAG = "car_notification_header";

    private final PackageManager mPackageManager;
    private final int mDefaultTextColor;
    private final String mSeparatorText;

    private boolean mIsHeadsUp;
    private ImageView mIconView;
    private TextView mHeaderTextView;
    private DateTimeView mTimeView;

    public CarNotificationHeaderView(Context context) {
        super(context);
    }

    public CarNotificationHeaderView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(attrs);
    }

    public CarNotificationHeaderView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init(attrs);
    }

    public CarNotificationHeaderView(Context context, AttributeSet attrs, int defStyleAttr,
            int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init(attrs);
    }

    {
        mPackageManager = getContext().getPackageManager();
        mDefaultTextColor = getContext().getColor(R.color.primary_text_color);
        mSeparatorText = getContext().getString(R.string.header_text_separator);
        inflate(getContext(), R.layout.car_notification_header_view, this);
    }

    private void init(AttributeSet attrs) {
        TypedArray attributes =
                getContext().obtainStyledAttributes(attrs, R.styleable.CarNotificationHeaderView);
        mIsHeadsUp =
                attributes.getBoolean(R.styleable.CarNotificationHeaderView_isHeadsUp,
                        /* defValue= */ false);
        attributes.recycle();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mIconView = findViewById(R.id.app_icon);
        mHeaderTextView = findViewById(R.id.header_text);
        mTimeView = findViewById(R.id.time);
        mTimeView.setShowRelativeTime(true);
    }

    /**
     * Binds the notification header that contains the issuer app icon and name.
     *
     * @param alertEntry the notification to be bound.
     * @param isInGroup  whether this notification is part of a grouped notification.
     */
    public void bind(AlertEntry alertEntry, boolean isInGroup) {
        if (isInGroup) {
            // if the notification is part of a group, individual headers are not shown
            // instead, there is a header for the entire group in the group notification template
            return;
        }

        Notification notification = alertEntry.getNotification();
        StatusBarNotification sbn = alertEntry.getStatusBarNotification();

        Context packageContext = sbn.getPackageContext(getContext());

        // app icon
        mIconView.setVisibility(View.VISIBLE);
        Drawable drawable = notification.getSmallIcon().loadDrawable(packageContext);
        mIconView.setImageDrawable(drawable);

        StringBuilder stringBuilder = new StringBuilder();

        // app name
        mHeaderTextView.setVisibility(View.VISIBLE);
        String appName = loadHeaderAppName(sbn);

        if (mIsHeadsUp) {
            mHeaderTextView.setText(appName);
            mTimeView.setVisibility(View.GONE);
            return;
        }

        stringBuilder.append(appName);
        Bundle extras = notification.extras;

        // optional field: sub text
        if (!TextUtils.isEmpty(extras.getCharSequence(Notification.EXTRA_SUB_TEXT))) {
            stringBuilder.append(mSeparatorText);
            stringBuilder.append(extras.getCharSequence(Notification.EXTRA_SUB_TEXT));
        }

        // optional field: content info
        if (!TextUtils.isEmpty(extras.getCharSequence(Notification.EXTRA_INFO_TEXT))) {
            stringBuilder.append(mSeparatorText);
            stringBuilder.append(extras.getCharSequence(Notification.EXTRA_INFO_TEXT));
        }

        // optional field: time
        if (notification.showsTime()) {
            stringBuilder.append(mSeparatorText);
            mTimeView.setVisibility(View.VISIBLE);
            mTimeView.setTime(notification.when);
        }

        mHeaderTextView.setText(BidiFormatter.getInstance().unicodeWrap(stringBuilder,
                TextDirectionHeuristics.LOCALE));
    }

    /**
     * Sets the color for the small icon.
     */
    public void setSmallIconColor(@ColorInt int color) {
        mIconView.setColorFilter(color);
    }

    /**
     * Sets the header text color.
     */
    public void setHeaderTextColor(@ColorInt int color) {
        mHeaderTextView.setTextColor(color);
    }

    /**
     * Sets the text color for the time field.
     */
    public void setTimeTextColor(@ColorInt int color) {
        mTimeView.setTextColor(color);
    }

    /**
     * Resets the notification header empty.
     */
    public void reset() {
        mIconView.setVisibility(View.GONE);
        mIconView.setImageDrawable(null);
        setSmallIconColor(mDefaultTextColor);

        mHeaderTextView.setVisibility(View.GONE);
        mHeaderTextView.setText(null);
        setHeaderTextColor(mDefaultTextColor);

        mTimeView.setVisibility(View.GONE);
        mTimeView.setTime(0);
        setTimeTextColor(mDefaultTextColor);
    }

    /**
     * Fetches the application label given the notification. If the notification is a system
     * generated message notification that is posting on behalf of another application, that
     * application's name is used.
     *
     * The system permission {@link android.Manifest.permission#SUBSTITUTE_NOTIFICATION_APP_NAME}
     * is required to post on behalf of another application. The notification extra should also
     * contain a key {@link Notification#EXTRA_SUBSTITUTE_APP_NAME} with the value of
     * the appropriate application name.
     *
     * @return application label. Returns {@code null} when application name is not found.
     */
    @Nullable
    private String loadHeaderAppName(StatusBarNotification sbn) {
        final Context packageContext = sbn.getPackageContext(mContext);
        final PackageManager pm = packageContext.getPackageManager();
        final Notification notification = sbn.getNotification();
        CharSequence name = pm.getApplicationLabel(packageContext.getApplicationInfo());
        if (notification.extras.containsKey(EXTRA_SUBSTITUTE_APP_NAME)) {
            // only system packages which lump together a bunch of unrelated stuff
            // may substitute a different name to make the purpose of the
            // notification more clear. the correct package label should always
            // be accessible via SystemUI.
            final String subName = notification.extras.getString(EXTRA_SUBSTITUTE_APP_NAME);
            final String pkg = sbn.getPackageName();
            if (PackageManager.PERMISSION_GRANTED == pm.checkPermission(
                    android.Manifest.permission.SUBSTITUTE_NOTIFICATION_APP_NAME, pkg)) {
                name = subName;
            } else {
                Log.w(TAG, "warning: pkg "
                        + pkg + " attempting to substitute app name '" + subName
                        + "' without holding perm "
                        + android.Manifest.permission.SUBSTITUTE_NOTIFICATION_APP_NAME);
            }
        }
        if (TextUtils.isEmpty(name)) {
            return null;
        }
        return String.valueOf(name);
    }
}
