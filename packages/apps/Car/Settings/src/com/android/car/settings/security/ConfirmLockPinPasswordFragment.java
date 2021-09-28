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

package com.android.car.settings.security;

import android.content.Context;
import android.os.Bundle;
import android.os.UserHandle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import com.android.car.settings.R;
import com.android.car.settings.common.BaseFragment;
import com.android.internal.widget.LockscreenCredential;
import com.android.internal.widget.TextViewInputDisabler;

/**
 * Fragment for confirming existing lock PIN or password.  The containing activity must implement
 * CheckLockListener.
 */
public class ConfirmLockPinPasswordFragment extends BaseFragment {

    private static final String FRAGMENT_TAG_CHECK_LOCK_WORKER = "check_lock_worker";
    private static final String EXTRA_IS_PIN = "extra_is_pin";

    private PinPadView mPinPad;
    private EditText mPasswordField;
    private TextView mMsgView;

    private CheckLockWorker mCheckLockWorker;
    private CheckLockListener mCheckLockListener;

    private int mUserId;
    private boolean mIsPin;
    private LockscreenCredential mEnteredPassword;

    private ConfirmLockLockoutHelper mConfirmLockLockoutHelper;

    private TextViewInputDisabler mPasswordEntryInputDisabler;
    private InputMethodManager mImm;

    /**
     * Factory method for creating fragment in PIN mode.
     */
    public static ConfirmLockPinPasswordFragment newPinInstance() {
        ConfirmLockPinPasswordFragment patternFragment = new ConfirmLockPinPasswordFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(EXTRA_IS_PIN, true);
        patternFragment.setArguments(bundle);
        return patternFragment;
    }

    /**
     * Factory method for creating fragment in password mode.
     */
    public static ConfirmLockPinPasswordFragment newPasswordInstance() {
        ConfirmLockPinPasswordFragment patternFragment = new ConfirmLockPinPasswordFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(EXTRA_IS_PIN, false);
        patternFragment.setArguments(bundle);
        return patternFragment;
    }

    @Override
    @LayoutRes
    protected int getLayoutId() {
        return mIsPin ? R.layout.confirm_lock_pin : R.layout.confirm_lock_password;
    }

    @Override
    @StringRes
    protected int getTitleId() {
        return R.string.security_settings_title;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        if ((getActivity() instanceof CheckLockListener)) {
            mCheckLockListener = (CheckLockListener) getActivity();
        } else {
            throw new RuntimeException("The activity must implement CheckLockListener");
        }

        mUserId = UserHandle.myUserId();
        mConfirmLockLockoutHelper = ConfirmLockLockoutHelper.getInstance(requireContext(), mUserId);
        mImm = requireContext().getSystemService(InputMethodManager.class);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Bundle args = getArguments();
        if (args != null) {
            mIsPin = args.getBoolean(EXTRA_IS_PIN);
        }
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mPasswordField = view.findViewById(R.id.password_entry);
        mPasswordEntryInputDisabler = new TextViewInputDisabler(mPasswordField);
        mMsgView = view.findViewById(R.id.message);
        mConfirmLockLockoutHelper.setConfirmLockUIController(
                new ConfirmLockLockoutHelper.ConfirmLockUIController() {
                    @Override
                    public void setErrorText(String text) {
                        mMsgView.setText(text);
                    }

                    @Override
                    public void refreshUI(boolean isLockedOut) {
                        if (mIsPin) {
                            updatePinEntry(isLockedOut);
                        } else {
                            updatePasswordEntry(isLockedOut);
                        }
                    }
                });

        if (mIsPin) {
            initPinView(view);
        } else {
            initPasswordView();
        }

        if (savedInstanceState != null) {
            mCheckLockWorker = (CheckLockWorker) getFragmentManager().findFragmentByTag(
                    FRAGMENT_TAG_CHECK_LOCK_WORKER);
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        if (mCheckLockWorker != null) {
            mCheckLockWorker.setListener(this::onCheckCompleted);
        }
    }

    @Override
    public void onResume() {
        super.onResume();

        mConfirmLockLockoutHelper.onResumeUI();
    }

    @Override
    public void onPause() {
        super.onPause();

        mConfirmLockLockoutHelper.onPauseUI();
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mCheckLockWorker != null) {
            mCheckLockWorker.setListener(null);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mPasswordField.setText(null);

        PasswordHelper.zeroizeCredentials(mEnteredPassword);
    }

    private void initCheckLockWorker() {
        if (mCheckLockWorker == null) {
            mCheckLockWorker = new CheckLockWorker();
            mCheckLockWorker.setListener(this::onCheckCompleted);

            getFragmentManager()
                    .beginTransaction()
                    .add(mCheckLockWorker, FRAGMENT_TAG_CHECK_LOCK_WORKER)
                    .commitNow();
        }
    }

    private LockscreenCredential getEnteredPassword() {
        if (mIsPin) {
            return LockscreenCredential.createPinOrNone(mPasswordField.getText());
        } else {
            return LockscreenCredential.createPasswordOrNone(mPasswordField.getText());
        }
    }

    private void initPinView(View view) {
        mPinPad = view.findViewById(R.id.pin_pad);

        PinPadView.PinPadClickListener pinPadClickListener = new PinPadView.PinPadClickListener() {
            @Override
            public void onDigitKeyClick(String digit) {
                clearError();
                mPasswordField.append(digit);
            }

            @Override
            public void onBackspaceClick() {
                clearError();
                if (!TextUtils.isEmpty(mPasswordField.getText())) {
                    mPasswordField.getText().delete(mPasswordField.getSelectionEnd() - 1,
                            mPasswordField.getSelectionEnd());
                }
            }

            @Override
            public void onEnterKeyClick() {
                mEnteredPassword = getEnteredPassword();
                if (!mEnteredPassword.isNone()) {
                    initCheckLockWorker();
                    mPinPad.setEnabled(false);
                    mCheckLockWorker.checkPinPassword(mUserId, mEnteredPassword);
                }
            }
        };

        mPinPad.setPinPadClickListener(pinPadClickListener);
    }

    private void initPasswordView() {
        mPasswordField.setOnEditorActionListener((textView, actionId, keyEvent) -> {
            // Check if this was the result of hitting the enter or "done" key.
            if (actionId == EditorInfo.IME_NULL
                    || actionId == EditorInfo.IME_ACTION_DONE
                    || actionId == EditorInfo.IME_ACTION_NEXT) {

                initCheckLockWorker();
                if (!mCheckLockWorker.isCheckInProgress()) {
                    mEnteredPassword = getEnteredPassword();
                    mCheckLockWorker.checkPinPassword(mUserId, mEnteredPassword);
                }
                return true;
            }
            return false;
        });

        mPasswordField.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {
            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void afterTextChanged(Editable s) {
                clearError();
            }
        });
    }

    private void clearError() {
        if (!TextUtils.isEmpty(mMsgView.getText())) {
            mMsgView.setText("");
        }
    }

    private void hideKeyboard() {
        View currentFocus = getActivity().getCurrentFocus();
        if (currentFocus == null) {
            currentFocus = getActivity().getWindow().getDecorView();
        }

        if (currentFocus != null) {
            InputMethodManager inputMethodManager =
                    (InputMethodManager) currentFocus.getContext()
                            .getSystemService(Context.INPUT_METHOD_SERVICE);
            inputMethodManager
                    .hideSoftInputFromWindow(currentFocus.getWindowToken(), 0);
        }
    }

    @VisibleForTesting
    void onCheckCompleted(boolean lockMatched, int timeoutMs) {
        if (lockMatched) {
            mCheckLockListener.onLockVerified(mEnteredPassword);
        } else {
            if (timeoutMs > 0) {
                mConfirmLockLockoutHelper.onCheckCompletedWithTimeout(timeoutMs);
            } else {
                mMsgView.setText(
                        mIsPin ? R.string.lockscreen_wrong_pin
                                : R.string.lockscreen_wrong_password);
                if (mIsPin) {
                    mPinPad.setEnabled(true);
                }
            }
        }

        if (!mIsPin) {
            hideKeyboard();
        }
    }

    private void updatePasswordEntry(boolean isLockedOut) {
        mPasswordField.setEnabled(!isLockedOut);
        mPasswordEntryInputDisabler.setInputEnabled(!isLockedOut);
        if (isLockedOut) {
            if (mImm != null) {
                mImm.hideSoftInputFromWindow(mPasswordField.getWindowToken(), /* flags= */ 0);
            }
        } else {
            mPasswordField.requestFocus();
        }
    }

    private void updatePinEntry(boolean isLockedOut) {
        mPinPad.setEnabled(!isLockedOut);
        if (isLockedOut) {
            if (mImm != null) {
                mImm.hideSoftInputFromWindow(mPinPad.getWindowToken(), /* flags= */ 0);
            }
        } else {
            mPinPad.requestFocus();
        }
    }
}
