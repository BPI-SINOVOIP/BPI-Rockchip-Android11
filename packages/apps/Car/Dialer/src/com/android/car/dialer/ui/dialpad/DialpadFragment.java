/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.car.dialer.ui.dialpad;

import android.annotation.CallSuper;
import android.media.AudioManager;
import android.media.ToneGenerator;
import android.os.Bundle;
import android.provider.CallLog;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.apps.common.util.ViewUtils;
import com.android.car.dialer.R;
import com.android.car.dialer.log.L;
import com.android.car.dialer.telecom.UiCallManager;
import com.android.car.dialer.ui.dialpad.DialpadRestrictionViewModel.DialpadUxrMode;
import com.android.car.dialer.ui.view.ContactAvatarOutputlineProvider;
import com.android.car.telephony.common.Contact;
import com.android.car.telephony.common.InMemoryPhoneBook;
import com.android.car.telephony.common.PhoneNumber;
import com.android.car.telephony.common.TelecomUtils;
import com.android.car.ui.recyclerview.CarUiRecyclerView;
import com.android.car.ui.toolbar.ToolbarController;
import com.android.car.uxr.LifeCycleObserverUxrContentLimiter;
import com.android.car.uxr.UxrContentLimiterImpl;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.collect.ImmutableMap;

/**
 * Fragment that controls the dialpad.
 */
public class DialpadFragment extends AbstractDialpadFragment {
    private static final String TAG = "CD.DialpadFragment";

    private static final String DIALPAD_MODE_KEY = "DIALPAD_MODE_KEY";
    private static final int MODE_DIAL = 1;
    private static final int MODE_EMERGENCY = 2;

    @VisibleForTesting
    static final int MAX_DIAL_NUMBER = 20;

    private static final int TONE_RELATIVE_VOLUME = 80;
    private static final int TONE_LENGTH_INFINITE = -1;
    private final ImmutableMap<Integer, Integer> mToneMap =
            ImmutableMap.<Integer, Integer>builder()
                    .put(KeyEvent.KEYCODE_1, ToneGenerator.TONE_DTMF_1)
                    .put(KeyEvent.KEYCODE_2, ToneGenerator.TONE_DTMF_2)
                    .put(KeyEvent.KEYCODE_3, ToneGenerator.TONE_DTMF_3)
                    .put(KeyEvent.KEYCODE_4, ToneGenerator.TONE_DTMF_4)
                    .put(KeyEvent.KEYCODE_5, ToneGenerator.TONE_DTMF_5)
                    .put(KeyEvent.KEYCODE_6, ToneGenerator.TONE_DTMF_6)
                    .put(KeyEvent.KEYCODE_7, ToneGenerator.TONE_DTMF_7)
                    .put(KeyEvent.KEYCODE_8, ToneGenerator.TONE_DTMF_8)
                    .put(KeyEvent.KEYCODE_9, ToneGenerator.TONE_DTMF_9)
                    .put(KeyEvent.KEYCODE_0, ToneGenerator.TONE_DTMF_0)
                    .put(KeyEvent.KEYCODE_STAR, ToneGenerator.TONE_DTMF_S)
                    .put(KeyEvent.KEYCODE_POUND, ToneGenerator.TONE_DTMF_P)
                    .build();
    private final TypeDownResultsAdapter mAdapter = new TypeDownResultsAdapter();

    private TypeDownResultsViewModel mTypeDownResultsViewModel;
    private TextView mTitleView;
    @Nullable
    private TextView mDisplayName;
    @Nullable
    private CarUiRecyclerView mRecyclerView;
    @Nullable
    private TextView mLabel;
    @Nullable
    private ImageView mAvatar;
    private ImageButton mDeleteButton;
    @Nullable
    private View mRestrictedDialingLabel;

    private int mMode;
    private boolean mHasTypeDown;

    private ToneGenerator mToneGenerator;

    private LifeCycleObserverUxrContentLimiter mUxrContentLimiter;

    /**
     * Creates a new instance of the {@link DialpadFragment} which is used for dialing a number.
     */
    public static DialpadFragment newPlaceCallDialpad() {
        DialpadFragment fragment = newDialpad(MODE_DIAL);
        return fragment;
    }

    /**
     * Creates a new instance used for emergency dialing.
     */
    public static DialpadFragment newEmergencyDialpad() {
        return newDialpad(MODE_EMERGENCY);
    }

    private static DialpadFragment newDialpad(int mode) {
        DialpadFragment fragment = new DialpadFragment();

        Bundle args = new Bundle();
        args.putInt(DIALPAD_MODE_KEY, mode);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mMode = getArguments().getInt(DIALPAD_MODE_KEY);
        L.d(TAG, "onCreate mode: %s", mMode);
        mToneGenerator = new ToneGenerator(AudioManager.STREAM_DTMF, TONE_RELATIVE_VOLUME);

        int limit = getResources().getInteger(R.integer.config_type_down_list_limit);
        mAdapter.setUnrestrictedItemCount(limit);
        mAdapter.setOnItemClickedListener(item -> clearDialedNumber());
        mTypeDownResultsViewModel = ViewModelProviders.of(this).get(
                TypeDownResultsViewModel.class);
        mTypeDownResultsViewModel.getContactSearchResults().observe(this,
                contactResults -> mAdapter.setData(contactResults));
        mTypeDownResultsViewModel.getSortOrderLiveData().observe(this,
                v -> mAdapter.setSortMethod(v));

        mUxrContentLimiter = new LifeCycleObserverUxrContentLimiter(
                new UxrContentLimiterImpl(getContext(), R.xml.uxr_config));
        getLifecycle().addObserver(mUxrContentLimiter);
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        mHasTypeDown = getResources().getBoolean(R.bool.config_show_type_down_list_on_dialpad);
        View rootView = inflater.inflate(mHasTypeDown ? R.layout.dialpad_fragment_with_type_down
                : R.layout.dialpad_fragment_without_type_down, container, false);

        mTitleView = rootView.findViewById(R.id.title);
        mTitleView.setTextAppearance(
                mMode == MODE_EMERGENCY ? R.style.TextAppearance_EmergencyDialNumber
                        : R.style.TextAppearance_DialNumber);
        mDisplayName = rootView.findViewById(R.id.display_name);
        mRecyclerView = rootView.findViewById(R.id.list_view);
        if (mRecyclerView != null) {
            mRecyclerView.setAdapter(mAdapter);
        }
        mLabel = rootView.findViewById(R.id.label);
        mAvatar = rootView.findViewById(R.id.dialpad_contact_avatar);
        if (mAvatar != null) {
            mAvatar.setOutlineProvider(ContactAvatarOutputlineProvider.get());
        }
        mRestrictedDialingLabel = rootView.findViewById(R.id.restricted_dialing_mode_label);

        View callButton = rootView.findViewById(R.id.call_button);
        callButton.setOnClickListener(v -> {
            if (!TextUtils.isEmpty(getNumber().toString())) {
                UiCallManager.get().placeCall(getNumber().toString());
                // Update dialed number UI later in onResume() when in call intent is handled.
                getNumber().setLength(0);
            } else {
                setDialedNumber(CallLog.Calls.getLastOutgoingCall(getContext()));
            }
        });

        callButton.addOnUnhandledKeyEventListener((v, event) -> {
            if (event.getKeyCode() == KeyEvent.KEYCODE_CALL) {
                // Use onKeyDown/Up instead of performClick() because it animates the ripple
                if (event.getAction() == KeyEvent.ACTION_DOWN) {
                    callButton.onKeyDown(KeyEvent.KEYCODE_ENTER, event);
                } else if (event.getAction() == KeyEvent.ACTION_UP) {
                    callButton.onKeyUp(KeyEvent.KEYCODE_ENTER, event);
                }
                return true;
            } else {
                return false;
            }
        });

        mDeleteButton = rootView.findViewById(R.id.delete_button);
        mDeleteButton.setOnClickListener(v -> removeLastDigit());
        mDeleteButton.setOnLongClickListener(v -> {
            clearDialedNumber();
            return true;
        });

        mUxrContentLimiter.setAdapter(mAdapter);

        return rootView;
    }

    @Override
    protected void setupToolbar(ToolbarController toolbar) {
        // Only setup the actionbar if we're in dial mode.
        // In all the other modes, there will be another fragment in the activity
        // at the same time, and we don't want to mess up it's action bar.
        if (mMode == MODE_DIAL) {
            super.setupToolbar(toolbar);
        }
    }

    @Override
    public void onKeypadKeyLongPressed(@KeypadFragment.DialKeyCode int keycode) {
        switch (keycode) {
            case KeyEvent.KEYCODE_0:
                removeLastDigit();
                appendDialedNumber("+");
                break;
            case KeyEvent.KEYCODE_STAR:
                removeLastDigit();
                appendDialedNumber(",");
                break;
            case KeyEvent.KEYCODE_1:
                UiCallManager.get().callVoicemail();
                break;
            default:
                break;
        }
    }

    @Override
    void playTone(int keycode) {
        L.d(TAG, "start key pressed tone for %s", keycode);
        mToneGenerator.startTone(mToneMap.get(keycode), TONE_LENGTH_INFINITE);
    }

    @Override
    void stopAllTones() {
        L.d(TAG, "stop key pressed tone");
        mToneGenerator.stopTone();
    }

    @Override
    void presentDialedNumber(@NonNull String number, @NonNull DialpadUxrMode dialpadUxrMode) {
        if (getView() == null) {
            return;
        }

        if (number.length() == 0) {
            mTitleView.setGravity(Gravity.CENTER);
            if (dialpadUxrMode == DialpadUxrMode.UNRESTRICTED) {
                mTitleView.setText(
                        mMode == MODE_DIAL ? R.string.dial_a_number
                                : R.string.emergency_call_description);
            } else {
                @StringRes int messageId;
                if (mMode == MODE_EMERGENCY || dialpadUxrMode == DialpadUxrMode.RESTRICTED) {
                    messageId = R.string.emergency_call_description;
                } else if (dialpadUxrMode == DialpadUxrMode.DISABLED) {
                    messageId = R.string.dialing_disabled_warning;
                } else {
                    messageId = R.string.dial_a_number;
                }
                mTitleView.setText(messageId);
            }
            ViewUtils.setVisible(mDeleteButton, false);
        } else {
            mTitleView.setGravity(
                    getResources().getInteger(R.integer.config_dialed_number_gravity));
            if (number.length() <= MAX_DIAL_NUMBER) {
                mTitleView.setText(TelecomUtils.getFormattedNumber(getContext(), number));
            } else {
                mTitleView.setText(number.substring(number.length() - MAX_DIAL_NUMBER));
            }
            ViewUtils.setVisible(mDeleteButton, true);
        }

        if (mHasTypeDown) {
            resetContactInfo();
            ViewUtils.setVisible(mRecyclerView, true);

            // In case of emergency or restricted dialing clean the type-down.
            mTypeDownResultsViewModel.setSearchQuery(
                    mMode == MODE_DIAL && dialpadUxrMode == DialpadUxrMode.UNRESTRICTED
                            ? number : "");
        } else {
            presentContactInfo(number);
        }
    }

    private void presentContactInfo(@NonNull String number) {
        Contact contact = InMemoryPhoneBook.get().lookupContactEntry(number);
        ViewUtils.setText(mDisplayName, contact == null ? "" : contact.getDisplayName());
        if (contact != null && getResources().getBoolean(
                R.bool.config_show_detailed_user_profile_on_dialpad)) {
            presentContactDetail(contact, number);
        } else {
            resetContactInfo();
        }
    }

    private void presentContactDetail(@Nullable Contact contact, @NonNull String number) {
        PhoneNumber phoneNumber = contact.getPhoneNumber(getContext(), number);
        CharSequence readableLabel = phoneNumber.getReadableLabel(
                getContext().getResources());
        ViewUtils.setText(mLabel, phoneNumber.isPrimary() ? getContext().getString(
                R.string.primary_number_description, readableLabel) : readableLabel);
        ViewUtils.setVisible(mLabel, true);

        TelecomUtils.setContactBitmapAsync(getContext(), mAvatar, contact,
                mAdapter.getSortMethod());
        ViewUtils.setVisible(mAvatar, true);
    }

    private void resetContactInfo() {
        ViewUtils.setVisible(mLabel, false);
        ViewUtils.setVisible(mAvatar, false);
    }

    @CallSuper
    @Override
    protected void onDialpadUxrModeChange(DialpadUxrMode dialpadUxrMode) {
        super.onDialpadUxrModeChange(dialpadUxrMode);

        boolean isRestrictedOrEmergencyMode =
                mMode == MODE_EMERGENCY ||  dialpadUxrMode != DialpadUxrMode.UNRESTRICTED;
        mTitleView.setTextAppearance(isRestrictedOrEmergencyMode
                ? R.style.TextAppearance_EmergencyDialNumber : R.style.TextAppearance_DialNumber);

        ViewUtils.setVisible(
                mRestrictedDialingLabel, dialpadUxrMode != DialpadUxrMode.UNRESTRICTED);
    }
}
