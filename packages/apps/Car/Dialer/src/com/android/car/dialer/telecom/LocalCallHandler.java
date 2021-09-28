/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.dialer.telecom;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.telecom.Call;
import android.telecom.CallAudioState;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import com.android.car.dialer.log.L;

import com.google.common.base.Predicate;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Binds to the {@link InCallServiceImpl}, and upon establishing a connection, handles call list
 * change and call audio state change.
 */
public class LocalCallHandler {
    private static final String TAG = "CD.CallHandler";
    private final Context mApplicationContext;
    private final Call.Callback mRingingCallStateChangeCallback;

    private final MutableLiveData<CallAudioState> mCallAudioStateLiveData = new MutableLiveData<>();
    private final InCallServiceImpl.CallAudioStateCallback mCallAudioStateCallback =
            mCallAudioStateLiveData::setValue;

    private final MutableLiveData<List<Call>> mCallListLiveData;
    private final MutableLiveData<Call> mIncomingCallLiveData;
    private final MutableLiveData<List<Call>> mOngoingCallListLiveData;
    private final InCallServiceImpl.ActiveCallListChangedCallback mActiveCallListChangedCallback =
            new InCallServiceImpl.ActiveCallListChangedCallback() {
                @Override
                public boolean onTelecomCallAdded(Call telecomCall) {
                    notifyCallAdded(telecomCall);
                    notifyCallListChanged();
                    return false;
                }

                @Override
                public boolean onTelecomCallRemoved(Call telecomCall) {
                    notifyCallRemoved(telecomCall);
                    notifyCallListChanged();
                    return false;
                }
            };

    private InCallServiceImpl mInCallService;
    private final ServiceConnection mInCallServiceConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder binder) {
            L.d(TAG, "onServiceConnected: %s, service: %s", name, binder);
            mInCallService = ((InCallServiceImpl.LocalBinder) binder).getService();
            for (Call call : mInCallService.getCalls()) {
                notifyCallAdded(call);
            }
            mInCallService.addActiveCallListChangedCallback(mActiveCallListChangedCallback);
            mInCallService.addCallAudioStateChangedCallback(mCallAudioStateCallback);
            notifyCallListChanged();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            L.d(TAG, "onServiceDisconnected: %s", name);
            mInCallService = null;
        }
    };

    /**
     * Initiate a LocalCallHandler.
     *
     * @param applicationContext Application context.
     */
    public LocalCallHandler(Context applicationContext) {
        mApplicationContext = applicationContext;

        mCallListLiveData = new MutableLiveData<>();
        mIncomingCallLiveData = new MutableLiveData<>();
        mOngoingCallListLiveData = new MutableLiveData<>();

        mRingingCallStateChangeCallback = new Call.Callback() {
            @Override
            public void onStateChanged(Call call, int state) {
                // Don't show in call activity by declining a ringing call to avoid UI flashing.
                if (state != Call.STATE_DISCONNECTED) {
                    notifyCallListChanged();
                }
                call.unregisterCallback(this);
            }
        };

        Intent intent = new Intent(mApplicationContext, InCallServiceImpl.class);
        intent.setAction(InCallServiceImpl.ACTION_LOCAL_BIND);
        mApplicationContext.bindService(intent, mInCallServiceConnection, Context.BIND_AUTO_CREATE);
    }

    /** Returns the {@link LiveData} which monitors the call audio state change. */
    public LiveData<CallAudioState> getCallAudioStateLiveData() {
        return mCallAudioStateLiveData;
    }

    /** Returns the {@link LiveData} which monitors the call list. */
    @NonNull
    public LiveData<List<Call>> getCallListLiveData() {
        return mCallListLiveData;
    }

    /** Returns the {@link LiveData} which monitors the active call list. */
    @NonNull
    public LiveData<List<Call>> getOngoingCallListLiveData() {
        return mOngoingCallListLiveData;
    }

    /** Returns the {@link LiveData} which monitors the ringing call. */
    @NonNull
    public LiveData<Call> getIncomingCallLiveData() {
        return mIncomingCallLiveData;
    }

    /** Disconnects the {@link InCallServiceImpl} and cleanup. */
    public void tearDown() {
        mApplicationContext.unbindService(mInCallServiceConnection);
        if (mInCallService != null) {
            for (Call call : mInCallService.getCalls()) {
                notifyCallRemoved(call);
            }
            mInCallService.removeActiveCallListChangedCallback(mActiveCallListChangedCallback);
            mInCallService.removeCallAudioStateChangedCallback(mCallAudioStateCallback);
        }
        mInCallService = null;
    }

    /**
     * The call list has updated, notify change. It will update when:
     * <ul>
     *     <li> {@link InCallServiceImpl} has been bind, init the call list.
     *     <li> A call has been added to the {@link InCallServiceImpl}.
     *     <li> A call has been removed from the {@link InCallServiceImpl}.
     *     <li> A call has changed.
     * </ul>
     */
    private void notifyCallListChanged() {
        List<Call> callList = new ArrayList<>(mInCallService.getCalls());

        List<Call> activeCallList = filter(callList,
                call -> call != null && call.getState() != Call.STATE_RINGING);
        mOngoingCallListLiveData.setValue(activeCallList);

        Call ringingCall = firstMatch(callList,
                call -> call != null && call.getState() == Call.STATE_RINGING);
        mIncomingCallLiveData.setValue(ringingCall);

        mCallListLiveData.setValue(callList);
    }

    private void notifyCallAdded(Call call) {
        if (call.getState() == Call.STATE_RINGING) {
            call.registerCallback(mRingingCallStateChangeCallback);
        }
    }

    private void notifyCallRemoved(Call call) {
        call.unregisterCallback(mRingingCallStateChangeCallback);
    }

    @Nullable
    private static Call firstMatch(List<Call> callList, Predicate<Call> predicate) {
        List<Call> filteredResults = filter(callList, predicate);
        return filteredResults.isEmpty() ? null : filteredResults.get(0);
    }

    @NonNull
    private static List<Call> filter(List<Call> callList, Predicate<Call> predicate) {
        if (callList == null || predicate == null) {
            return Collections.emptyList();
        }

        List<Call> filteredResults = new ArrayList<>();
        for (Call call : callList) {
            if (predicate.apply(call)) {
                filteredResults.add(call);
            }
        }
        return filteredResults;
    }
}
