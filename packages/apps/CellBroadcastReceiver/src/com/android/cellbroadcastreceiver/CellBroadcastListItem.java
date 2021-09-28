/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.cellbroadcastreceiver;

import android.content.Context;
import android.database.Cursor;
import android.graphics.Typeface;
import android.provider.Telephony;
import android.telephony.SmsCbMessage;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.format.DateUtils;
import android.text.style.StyleSpan;
import android.util.AttributeSet;
import android.view.accessibility.AccessibilityEvent;
import android.widget.RelativeLayout;
import android.widget.TextView;

/**
 * This class manages the list item view for a single alert.
 */
public class CellBroadcastListItem extends RelativeLayout {

    private SmsCbMessage mCbMessage;

    private TextView mChannelView;
    private TextView mMessageView;
    private TextView mDateView;
    private Context mContext;

    public CellBroadcastListItem(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
    }

    SmsCbMessage getMessage() {
        return mCbMessage;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mChannelView = (TextView) findViewById(R.id.channel);
        mDateView = (TextView) findViewById(R.id.date);
        mMessageView = (TextView) findViewById(R.id.message);
    }

    /**
     * Only used for header binding.
     * @param message the message contents to bind
     */
    public void bind(SmsCbMessage message) {
        mCbMessage = message;
        mChannelView.setText(CellBroadcastResources.getDialogTitleResource(mContext, message));
        mDateView.setText(DateUtils.formatDateTime(getContext(), message.getReceivedTime(),
                DateUtils.FORMAT_NO_NOON_MIDNIGHT | DateUtils.FORMAT_SHOW_TIME
                        | DateUtils.FORMAT_ABBREV_ALL | DateUtils.FORMAT_SHOW_DATE
                        | DateUtils.FORMAT_CAP_AMPM));

        SpannableStringBuilder messageText = new SpannableStringBuilder(message.getMessageBody());
        try (Cursor cursor = mContext.getContentResolver().query(
                CellBroadcastContentProvider.CONTENT_URI,
                CellBroadcastDatabaseHelper.QUERY_COLUMNS,
                Telephony.CellBroadcasts.DELIVERY_TIME + "=?",
                new String[] {Long.toString(message.getReceivedTime())},
                null)) {
            if (cursor != null) {
                while (cursor.moveToNext()) {
                    if (cursor.getInt(cursor.getColumnIndexOrThrow(
                            Telephony.CellBroadcasts.MESSAGE_READ)) != 0) {
                        messageText.setSpan(new StyleSpan(Typeface.BOLD), 0, messageText.length(),
                                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
                        break;
                    }
                }
            }
        }

        mMessageView.setText(messageText);
    }

    @Override
    public boolean dispatchPopulateAccessibilityEvent(AccessibilityEvent event) {
        // Speak the date first, then channel name, then message body
        int flags = DateUtils.FORMAT_SHOW_TIME | DateUtils.FORMAT_SHOW_DATE;
        String dateTime = DateUtils.formatDateTime(getContext(), mCbMessage.getReceivedTime(),
                flags);

        event.getText().add(dateTime);
        mChannelView.dispatchPopulateAccessibilityEvent(event);
        mMessageView.dispatchPopulateAccessibilityEvent(event);
        return true;
    }
}
