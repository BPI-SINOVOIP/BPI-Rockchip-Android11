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

package com.android.car.dialer.ui.common;

import android.content.Context;
import android.text.format.DateUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MediatorLiveData;

import com.android.car.dialer.R;
import com.android.car.dialer.livedata.HeartBeatLiveData;
import com.android.car.dialer.log.L;
import com.android.car.dialer.ui.common.entity.UiCallLog;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.InMemoryPhoneBook;
import com.android.car.telephony.common.PhoneCallLog;
import com.android.car.telephony.common.TelecomUtils;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

/**
 * Represents a list of {@link UiCallLog}s and label {@link String}s for UI representation. This
 * live data gets data source from both call log and contact list. It also refresh itself on the
 * relative time in the body text.
 */
public class UiCallLogLiveData extends MediatorLiveData<List<Object>> {
    private static final String TAG = "CD.UiCallLogLiveData";

    private final ExecutorService mExecutorService;
    private Future<?> mRunnableFuture;
    private Context mContext;

    public UiCallLogLiveData(Context context,
            HeartBeatLiveData heartBeatLiveData,
            LiveData<List<PhoneCallLog>> callHistoryLiveData,
            LiveData<List<Contact>> contactListLiveData) {
        mContext = context;
        mExecutorService = Executors.newSingleThreadExecutor();

        addSource(callHistoryLiveData, this::onCallHistoryChanged);
        addSource(contactListLiveData,
                (contacts) -> onContactsChanged(callHistoryLiveData.getValue()));
        addSource(heartBeatLiveData, (trigger) -> updateRelativeTime());
    }

    private void onCallHistoryChanged(@Nullable List<PhoneCallLog> callLogs) {
        if (mRunnableFuture != null) {
            mRunnableFuture.cancel(true);
        }
        Runnable runnable = () -> postValue(convert(callLogs));
        mRunnableFuture = mExecutorService.submit(runnable);
    }

    private void onContactsChanged(List<PhoneCallLog> callLogs) {
        // When contacts change, do not set value to trigger an update when there are no
        // call logs loaded yet. An update will switch the loading state to loaded in the ViewModel.
        if (getValue() == null || getValue().isEmpty()) {
            return;
        }
        onCallHistoryChanged(callLogs);
    }

    private void updateRelativeTime() {
        boolean hasChanged = false;
        List<Object> uiCallLogs = getValue();
        if (uiCallLogs == null) {
            return;
        }
        for (Object object : uiCallLogs) {
            if (object instanceof UiCallLog) {
                UiCallLog uiCallLog = (UiCallLog) object;

                String newRelativeTime = getRelativeTime(uiCallLog.getMostRecentCallEndTimestamp());
                if (uiCallLog.setRelativeTime(newRelativeTime)) {
                    hasChanged = true;
                }
            }
        }

        if (hasChanged) {
            setValue(getValue());
        }
    }

    @NonNull
    private List<Object> convert(@Nullable List<PhoneCallLog> phoneCallLogs) {
        if (phoneCallLogs == null) {
            return Collections.emptyList();
        }
        List<Object> uiCallLogs = new ArrayList<>();
        String preHeader = null;

        InMemoryPhoneBook inMemoryPhoneBook = InMemoryPhoneBook.get();
        for (PhoneCallLog phoneCallLog : phoneCallLogs) {
            String header = getHeader(phoneCallLog.getLastCallEndTimestamp());
            if (preHeader == null || (!header.equals(preHeader))) {
                uiCallLogs.add(header);
            }
            preHeader = header;

            String number = phoneCallLog.getPhoneNumberString();
            String relativeTime = getRelativeTime(phoneCallLog.getLastCallEndTimestamp());
            if (TelecomUtils.isVoicemailNumber(mContext, number)) {
                String title = mContext.getString(R.string.voicemail);
                UiCallLog uiCallLog = new UiCallLog(title, title, number, null,
                        phoneCallLog.getAllCallRecords());
                uiCallLog.setRelativeTime(relativeTime);
                uiCallLogs.add(uiCallLog);
                continue;
            }

            // If InMemoryPhoneBook hasn't finished loading, there is still a chance that this
            // number can be found there later. So query will not be proceeded now.
            TelecomUtils.PhoneNumberInfo phoneNumberInfo =
                    TelecomUtils.lookupNumberInBackground(mContext, number);
            Contact contact = inMemoryPhoneBook.lookupContactByKey(
                    phoneNumberInfo.getLookupKey(),
                    phoneCallLog.getAccountName());

            UiCallLog uiCallLog = new UiCallLog(
                    phoneNumberInfo.getDisplayName(),
                    phoneNumberInfo.getDisplayNameAlt(),
                    number,
                    contact,
                    phoneCallLog.getAllCallRecords());

            uiCallLog.setRelativeTime(relativeTime);
            uiCallLog.setLabel(phoneNumberInfo.getTypeLabel());
            uiCallLogs.add(uiCallLog);
        }
        L.i(TAG, "phoneCallLog size: %d, uiCallLog size: %d",
                phoneCallLogs.size(), uiCallLogs.size());

        return uiCallLogs;
    }

    private String getRelativeTime(long millis) {
        boolean validTimestamp = millis > 0;

        return validTimestamp ? DateUtils.getRelativeTimeSpanString(
                millis, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS,
                DateUtils.FORMAT_ABBREV_RELATIVE).toString() : "";
    }

    private String getHeader(long calllogTime) {
        // Calllog times are acquired before getting currentTime, so calllogTime is always
        // less than currentTime
        if (DateUtils.isToday(calllogTime)) {
            return mContext.getResources().getString(R.string.call_log_header_today);
        }

        Calendar callLogCalender = Calendar.getInstance();
        callLogCalender.setTimeInMillis(calllogTime);
        callLogCalender.add(Calendar.DAY_OF_YEAR, 1);

        if (DateUtils.isToday(callLogCalender.getTimeInMillis())) {
            return mContext.getResources().getString(R.string.call_log_header_yesterday);
        }

        return mContext.getResources().getString(R.string.call_log_header_older);
    }
}
