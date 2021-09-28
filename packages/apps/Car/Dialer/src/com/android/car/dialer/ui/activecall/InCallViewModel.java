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

package com.android.car.dialer.ui.activecall;

import android.app.Application;
import android.content.Context;
import android.telecom.Call;
import android.telecom.CallAudioState;

import androidx.annotation.NonNull;
import androidx.core.util.Pair;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MediatorLiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.Transformations;

import com.android.car.arch.common.LiveDataFunctions;
import com.android.car.dialer.bluetooth.UiBluetoothMonitor;
import com.android.car.dialer.livedata.AudioRouteLiveData;
import com.android.car.dialer.livedata.CallDetailLiveData;
import com.android.car.dialer.livedata.CallStateLiveData;
import com.android.car.dialer.log.L;
import com.android.car.dialer.telecom.LocalCallHandler;
import com.android.car.telephony.common.CallDetail;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.InMemoryPhoneBook;

import com.google.common.collect.Lists;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * View model for {@link InCallActivity} and {@link OngoingCallFragment}. UI that doesn't belong to
 * in call page should use a different ViewModel.
 */
public class InCallViewModel extends AndroidViewModel {
    private static final String TAG = "CD.InCallViewModel";

    private final LocalCallHandler mLocalCallHandler;

    private final MutableLiveData<Boolean> mHasOngoingCallChangedLiveData;
    private final MediatorLiveData<List<Call>> mOngoingCallListLiveData;
    private final MutableLiveData<List<Call>> mConferenceCallListLiveData;;
    private final LiveData<List<CallDetail>> mConferenceCallDetailListLiveData;
    private final Comparator<Call> mCallComparator;

    private final CallDetailLiveData mCallDetailLiveData;
    private final LiveData<Integer> mCallStateLiveData;
    private final LiveData<Call> mPrimaryCallLiveData;
    private final LiveData<Call> mSecondaryCallLiveData;
    private final CallDetailLiveData mSecondaryCallDetailLiveData;
    private final LiveData<Pair<Call, Call>> mOngoingCallPairLiveData;
    private final LiveData<Integer> mAudioRouteLiveData;
    private final MutableLiveData<Boolean> mDialpadIsOpen;
    private final ShowOnholdCallLiveData mShowOnholdCall;
    private LiveData<Long> mCallConnectTimeLiveData;
    private LiveData<Long> mSecondaryCallConnectTimeLiveData;
    private LiveData<Pair<Integer, Long>> mCallStateAndConnectTimeLiveData;
    private LiveData<List<Contact>> mContactListLiveData;

    private final Context mContext;

    // Reuse the same instance so the callback won't be registered more than once.
    private final Call.Callback mCallStateChangedCallback = new Call.Callback() {
        @Override
        public void onStateChanged(Call call, int state) {
            L.d(TAG, "onStateChanged: %s", call);
            mHasOngoingCallChangedLiveData.setValue(true);
        }

        @Override
        public void onParentChanged(Call call, Call parent) {
            L.d(TAG, "onParentChanged %s", call);
            mHasOngoingCallChangedLiveData.setValue(true);
        }

        @Override
        public void onChildrenChanged(Call call, List<Call> children) {
            L.d(TAG, "onChildrenChanged %s", call);
            mHasOngoingCallChangedLiveData.setValue(true);
        }
    };

    public InCallViewModel(@NonNull Application application) {
        super(application);
        mContext = application.getApplicationContext();

        mLocalCallHandler = new LocalCallHandler(mContext);
        mCallComparator = new CallComparator();

        mConferenceCallListLiveData = new MutableLiveData<>();
        mHasOngoingCallChangedLiveData = new MutableLiveData<>();
        mOngoingCallListLiveData = new MediatorLiveData<>();
        mOngoingCallListLiveData.addSource(mHasOngoingCallChangedLiveData,
                changed -> recalculateOngoingCallList());
        mOngoingCallListLiveData.addSource(mLocalCallHandler.getOngoingCallListLiveData(),
                callList -> recalculateOngoingCallList());

        mConferenceCallDetailListLiveData = Transformations.map(mConferenceCallListLiveData,
                callList -> {
                    List<CallDetail> detailList = new ArrayList<>();
                    for (Call call : callList) {
                        detailList.add(CallDetail.fromTelecomCallDetail(call.getDetails()));
                    }
                    return detailList;
                });

        mCallDetailLiveData = new CallDetailLiveData();
        mPrimaryCallLiveData = Transformations.map(mOngoingCallListLiveData, input -> {
            Call call = input.isEmpty() ? null : input.get(0);
            mCallDetailLiveData.setTelecomCall(call);
            return call;
        });

        mCallStateLiveData = Transformations.switchMap(mPrimaryCallLiveData,
                input -> input != null ? new CallStateLiveData(input) : null);
        mCallConnectTimeLiveData = Transformations.map(mCallDetailLiveData, (details) -> {
            if (details == null) {
                return 0L;
            }
            return details.getConnectTimeMillis();
        });
        mCallStateAndConnectTimeLiveData =
                LiveDataFunctions.pair(mCallStateLiveData, mCallConnectTimeLiveData);

        mSecondaryCallDetailLiveData = new CallDetailLiveData();
        mSecondaryCallLiveData = Transformations.map(mOngoingCallListLiveData, callList -> {
            Call call = (callList != null && callList.size() > 1) ? callList.get(1) : null;
            mSecondaryCallDetailLiveData.setTelecomCall(call);
            return call;
        });

        mSecondaryCallConnectTimeLiveData = Transformations.map(mSecondaryCallDetailLiveData,
                details -> {
                    if (details == null) {
                        return 0L;
                    }
                    return details.getConnectTimeMillis();
                });

        mOngoingCallPairLiveData = LiveDataFunctions.pair(mPrimaryCallLiveData,
                mSecondaryCallLiveData);

        mAudioRouteLiveData = new AudioRouteLiveData(mContext);

        mDialpadIsOpen = new MutableLiveData<>();
        // Set initial value to avoid NPE
        mDialpadIsOpen.setValue(false);

        mShowOnholdCall = new ShowOnholdCallLiveData(mSecondaryCallLiveData, mDialpadIsOpen);

        mContactListLiveData = LiveDataFunctions.switchMapNonNull(
                UiBluetoothMonitor.get().getFirstHfpConnectedDevice(),
                device -> InMemoryPhoneBook.get().getContactsLiveDataByAccount(
                        device.getAddress()));
    }

    /** Merge primary and secondary calls into a conference */
    public void mergeConference() {
        Call call = mPrimaryCallLiveData.getValue();
        Call otherCall = mSecondaryCallLiveData.getValue();

        if (call == null || otherCall == null) {
            return;
        }
        call.conference(otherCall);
    }

    /** Returns the live data which monitors conference calls */
    public LiveData<List<CallDetail>> getConferenceCallDetailList() {
        return mConferenceCallDetailListLiveData;
    }

    /** Returns the live data which monitors all the calls. */
    public LiveData<List<Call>> getAllCallList() {
        return mLocalCallHandler.getCallListLiveData();
    }

    /** Returns the live data which monitors the current incoming call. */
    public LiveData<Call> getIncomingCall() {
        return mLocalCallHandler.getIncomingCallLiveData();
    }

    /** Returns {@link LiveData} for the ongoing call list which excludes the ringing call. */
    public LiveData<List<Call>> getOngoingCallList() {
        return mOngoingCallListLiveData;
    }

    /**
     * Returns the live data which monitors the primary call details.
     */
    public LiveData<CallDetail> getPrimaryCallDetail() {
        return mCallDetailLiveData;
    }

    /**
     * Returns the live data which monitors the primary call state.
     */
    public LiveData<Integer> getPrimaryCallState() {
        return mCallStateLiveData;
    }

    /**
     * Returns the live data which monitors the primary call state and the start time of the call.
     */
    public LiveData<Pair<Integer, Long>> getCallStateAndConnectTime() {
        return mCallStateAndConnectTimeLiveData;
    }

    /**
     * Returns the live data which monitor the primary call.
     * A primary call in the first call in the ongoing call list,
     * which is sorted based on {@link CallComparator}.
     */
    public LiveData<Call> getPrimaryCall() {
        return mPrimaryCallLiveData;
    }

    /**
     * Returns the live data which monitor the secondary call.
     * A secondary call in the second call in the ongoing call list,
     * which is sorted based on {@link CallComparator}.
     * The value will be null if there is no second call in the call list.
     */
    public LiveData<Call> getSecondaryCall() {
        return mSecondaryCallLiveData;
    }

    /**
     * Returns the live data which monitors the secondary call details.
     */
    public LiveData<CallDetail> getSecondaryCallDetail() {
        return mSecondaryCallDetailLiveData;
    }

    /**
     * Returns the live data which monitors the secondary call connect time.
     */
    public LiveData<Long> getSecondaryCallConnectTime() {
        return mSecondaryCallConnectTimeLiveData;
    }

    /**
     * Returns the live data that monitors the primary and secondary calls.
     */
    public LiveData<Pair<Call, Call>> getOngoingCallPair() {
        return mOngoingCallPairLiveData;
    }

    /**
     * Returns current audio route.
     */
    public LiveData<Integer> getAudioRoute() {
        return mAudioRouteLiveData;
    }

    /**
     * Returns contact list.
     */
    public LiveData<List<Contact>> getContactListLiveData() {
        return mContactListLiveData;
    }

    /**
     * Returns current call audio state.
     */
    public LiveData<CallAudioState> getCallAudioState() {
        return mLocalCallHandler.getCallAudioStateLiveData();
    }

    /** Return the {@link MutableLiveData} for dialpad open state. */
    public MutableLiveData<Boolean> getDialpadOpenState() {
        return mDialpadIsOpen;
    }

    /** Return the livedata monitors onhold call status. */
    public LiveData<Boolean> shouldShowOnholdCall() {
        return mShowOnholdCall;
    }

    @Override
    protected void onCleared() {
        unregisterOngoingCallCallbacks();
        mLocalCallHandler.tearDown();
    }

    private void recalculateOngoingCallList() {
        L.d(TAG, "recalculate ongoing call list");
        unregisterOngoingCallCallbacks();

        List<Call> activeCallList = mLocalCallHandler.getOngoingCallListLiveData().getValue();
        if (activeCallList == null || activeCallList.isEmpty()) {
            mOngoingCallListLiveData.setValue(Collections.emptyList());
            mConferenceCallListLiveData.setValue(Collections.emptyList());
            return;
        }

        activeCallList.sort(mCallComparator);

        List<Call> conferenceList = new ArrayList<>();
        List<Call> ongoingCallList = new ArrayList<>();
        for (Call call : activeCallList) {
            call.registerCallback(mCallStateChangedCallback);
            if (call.getParent() != null) {
                conferenceList.add(call);
            } else {
                ongoingCallList.add(call);
            }
        }

        L.d(TAG, "size:" + activeCallList.size() + " activeList" + activeCallList);
        L.d(TAG, "conf:%s" + conferenceList, conferenceList.size());
        L.d(TAG, "ongoing:%s" + ongoingCallList, ongoingCallList.size());
        mConferenceCallListLiveData.setValue(conferenceList);
        mOngoingCallListLiveData.setValue(ongoingCallList);
    }

    /**
     * A call might be removed when bluetooth disconnects. The right time to unregister the callback
     * is when the ongoing call list changes or {@link InCallViewModel} gets destroyed.
     */
    private void unregisterOngoingCallCallbacks() {
        List<Call> ongoingCallList = mOngoingCallListLiveData.getValue();
        if (ongoingCallList != null) {
            for (Call call : ongoingCallList) {
                call.unregisterCallback(mCallStateChangedCallback);
            }
        }
    }

    private static class CallComparator implements Comparator<Call> {
        /**
         * The rank of call state. Used for sorting active calls. Rank is listed from lowest to
         * highest.
         */
        private static final List<Integer> CALL_STATE_RANK = Lists.newArrayList(
                Call.STATE_RINGING,
                Call.STATE_DISCONNECTED,
                Call.STATE_DISCONNECTING,
                Call.STATE_NEW,
                Call.STATE_CONNECTING,
                Call.STATE_SELECT_PHONE_ACCOUNT,
                Call.STATE_HOLDING,
                Call.STATE_ACTIVE,
                Call.STATE_DIALING);

        @Override
        public int compare(Call call, Call otherCall) {
            boolean callHasParent = call.getParent() != null;
            boolean otherCallHasParent = otherCall.getParent() != null;

            if (callHasParent && !otherCallHasParent) {
                return 1;
            } else if (!callHasParent && otherCallHasParent) {
                return -1;
            }
            int carCallRank = CALL_STATE_RANK.indexOf(call.getState());
            int otherCarCallRank = CALL_STATE_RANK.indexOf(otherCall.getState());

            return otherCarCallRank - carCallRank;
        }
    }

    private static class ShowOnholdCallLiveData extends MediatorLiveData<Boolean> {

        private final LiveData<Call> mSecondaryCallLiveData;
        private final MutableLiveData<Boolean> mDialpadIsOpen;

        private ShowOnholdCallLiveData(LiveData<Call> secondaryCallLiveData,
                MutableLiveData<Boolean> dialpadState) {
            mSecondaryCallLiveData = secondaryCallLiveData;
            mDialpadIsOpen = dialpadState;
            setValue(false);

            addSource(mSecondaryCallLiveData, v -> update());
            addSource(mDialpadIsOpen, v -> update());
        }

        private void update() {
            Boolean shouldShowOnholdCall = !mDialpadIsOpen.getValue();
            Call onholdCall = mSecondaryCallLiveData.getValue();
            if (shouldShowOnholdCall && onholdCall != null
                    && onholdCall.getState() == Call.STATE_HOLDING) {
                setValue(true);
            } else {
                setValue(false);
            }
        }

        @Override
        public void setValue(Boolean newValue) {
            // Only set value and notify observers when the value changes.
            if (getValue() != newValue) {
                super.setValue(newValue);
            }
        }
    }
}
