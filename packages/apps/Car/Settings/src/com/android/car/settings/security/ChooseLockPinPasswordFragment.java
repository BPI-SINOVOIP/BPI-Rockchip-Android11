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

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.UserHandle;
import android.text.Editable;
import android.text.Selection;
import android.text.Spannable;
import android.text.TextWatcher;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import com.android.car.settings.R;
import com.android.car.settings.common.BaseFragment;
import com.android.car.settings.common.Logger;
import com.android.car.ui.toolbar.MenuItem;
import com.android.car.ui.toolbar.ProgressBarController;
import com.android.internal.widget.LockscreenCredential;
import com.android.internal.widget.TextViewInputDisabler;

import java.util.Arrays;
import java.util.List;
import java.util.Objects;

/**
 * Fragment for choosing a lock password/pin.
 */
public class ChooseLockPinPasswordFragment extends BaseFragment {

    private static final String LOCK_OPTIONS_DIALOG_TAG = "lock_options_dialog_tag";
    private static final String FRAGMENT_TAG_SAVE_PASSWORD_WORKER = "save_password_worker";
    private static final String STATE_UI_STAGE = "state_ui_stage";
    private static final String STATE_FIRST_ENTRY = "state_first_entry";
    private static final Logger LOG = new Logger(ChooseLockPinPasswordFragment.class);
    private static final String EXTRA_IS_PIN = "extra_is_pin";

    private Stage mUiStage = Stage.Introduction;

    private int mUserId;
    private int mErrorCode = PasswordHelper.NO_ERROR;

    private boolean mIsPin;
    private boolean mIsAlphaMode;

    // Password currently in the input field
    private LockscreenCredential mCurrentEntry;
    // Existing password that user previously set
    private LockscreenCredential mExistingCredential;
    // Password must be entered twice.  This is what user entered the first time.
    private LockscreenCredential mFirstEntry;

    private PinPadView mPinPad;
    private TextView mHintMessage;
    private MenuItem mSecondaryButton;
    private MenuItem mPrimaryButton;
    private EditText mPasswordField;
    private ProgressBarController mProgressBar;

    private TextChangedHandler mTextChangedHandler = new TextChangedHandler();
    private TextViewInputDisabler mPasswordEntryInputDisabler;
    private SaveLockWorker mSaveLockWorker;
    private PasswordHelper mPasswordHelper;

    /**
     * Factory method for creating fragment in password mode
     */
    public static ChooseLockPinPasswordFragment newPasswordInstance() {
        ChooseLockPinPasswordFragment passwordFragment = new ChooseLockPinPasswordFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(EXTRA_IS_PIN, false);
        passwordFragment.setArguments(bundle);
        return passwordFragment;
    }

    /**
     * Factory method for creating fragment in Pin mode
     */
    public static ChooseLockPinPasswordFragment newPinInstance() {
        ChooseLockPinPasswordFragment passwordFragment = new ChooseLockPinPasswordFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(EXTRA_IS_PIN, true);
        passwordFragment.setArguments(bundle);
        return passwordFragment;
    }

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Arrays.asList(mPrimaryButton, mSecondaryButton);
    }

    @Override
    @LayoutRes
    protected int getLayoutId() {
        return mIsPin ? R.layout.choose_lock_pin : R.layout.choose_lock_password;
    }

    @Override
    @StringRes
    protected int getTitleId() {
        return mIsPin ? R.string.security_lock_pin : R.string.security_lock_password;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mUserId = UserHandle.myUserId();

        Bundle args = getArguments();
        if (args != null) {
            mIsPin = args.getBoolean(EXTRA_IS_PIN);
            mExistingCredential = args.getParcelable(PasswordHelper.EXTRA_CURRENT_SCREEN_LOCK);
            if (mExistingCredential != null) {
                mExistingCredential = mExistingCredential.duplicate();
            }
        }

        mPasswordHelper = new PasswordHelper(mIsPin);

        int passwordQuality = mPasswordHelper.getPasswordQuality();
        mIsAlphaMode = DevicePolicyManager.PASSWORD_QUALITY_ALPHABETIC == passwordQuality
                || DevicePolicyManager.PASSWORD_QUALITY_ALPHANUMERIC == passwordQuality
                || DevicePolicyManager.PASSWORD_QUALITY_COMPLEX == passwordQuality;

        if (savedInstanceState != null) {
            mUiStage = Stage.values()[savedInstanceState.getInt(STATE_UI_STAGE)];
            mFirstEntry = savedInstanceState.getParcelable(STATE_FIRST_ENTRY);
        }

        mPrimaryButton = new MenuItem.Builder(getContext())
                .setOnClickListener(i -> handlePrimaryButtonClick())
                .build();
        mSecondaryButton = new MenuItem.Builder(getContext())
                .setOnClickListener(i -> handleSecondaryButtonClick())
                .build();
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mPasswordField = view.findViewById(R.id.password_entry);
        mPasswordField.setOnEditorActionListener((textView, actionId, keyEvent) -> {
            // Check if this was the result of hitting the enter or "done" key
            if (actionId == EditorInfo.IME_NULL
                    || actionId == EditorInfo.IME_ACTION_DONE
                    || actionId == EditorInfo.IME_ACTION_NEXT) {
                handlePrimaryButtonClick();
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
                // Changing the text while error displayed resets to a normal state
                if (mUiStage == Stage.ConfirmWrong) {
                    mUiStage = Stage.NeedToConfirm;
                } else if (mUiStage == Stage.PasswordInvalid) {
                    mUiStage = Stage.Introduction;
                }
                // Schedule the UI update.
                if (isResumed()) {
                    mTextChangedHandler.notifyAfterTextChanged();
                }
            }
        });

        mPasswordEntryInputDisabler = new TextViewInputDisabler(mPasswordField);

        mHintMessage = view.findViewById(R.id.hint_text);

        if (mIsPin) {
            initPinView(view);
        } else {
            mPasswordField.requestFocus();
            InputMethodManager imm = (InputMethodManager)
                    getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.showSoftInput(mPasswordField, InputMethodManager.SHOW_IMPLICIT);
            }
        }

        // Re-attach to the exiting worker if there is one.
        if (savedInstanceState != null) {
            mSaveLockWorker = (SaveLockWorker) getFragmentManager().findFragmentByTag(
                    FRAGMENT_TAG_SAVE_PASSWORD_WORKER);
        }
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mProgressBar = getToolbar().getProgressBar();
    }

    @Override
    public void onStart() {
        super.onStart();
        updateStage(mUiStage);

        if (mSaveLockWorker != null) {
            mSaveLockWorker.setListener(this::onChosenLockSaveFinished);
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(STATE_UI_STAGE, mUiStage.ordinal());
        outState.putParcelable(STATE_FIRST_ENTRY, mFirstEntry);
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mSaveLockWorker != null) {
            mSaveLockWorker.setListener(null);
        }
        mProgressBar.setVisible(false);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mPasswordField.setText(null);

        PasswordHelper.zeroizeCredentials(mCurrentEntry, mExistingCredential, mFirstEntry);
    }

    /**
     * Append the argument to the end of the password entry field
     */
    private void appendToPasswordEntry(String text) {
        mPasswordField.append(text);
    }

    /**
     * Returns the string in the password entry field
     */
    @NonNull
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
                appendToPasswordEntry(digit);
            }

            @Override
            public void onBackspaceClick() {
                LockscreenCredential pin = getEnteredPassword();
                if (pin.size() > 0) {
                    mPasswordField.getText().delete(mPasswordField.getSelectionEnd() - 1,
                            mPasswordField.getSelectionEnd());
                }
                pin.zeroize();
            }

            @Override
            public void onEnterKeyClick() {
                handlePrimaryButtonClick();
            }
        };

        mPinPad.setPinPadClickListener(pinPadClickListener);
    }

    private boolean shouldEnableSubmit() {
        return getEnteredPassword().size() >= PasswordHelper.MIN_LENGTH
                && (mSaveLockWorker == null || mSaveLockWorker.isFinished());
    }

    private void updateSubmitButtonsState() {
        boolean enabled = shouldEnableSubmit();

        mPrimaryButton.setEnabled(enabled);
        if (mIsPin) {
            mPinPad.setEnterKeyEnabled(enabled);
        }
    }

    private void setPrimaryButtonText(@StringRes int textId) {
        mPrimaryButton.setTitle(textId);
    }

    private void setSecondaryButtonEnabled(boolean enabled) {
        mSecondaryButton.setEnabled(enabled);
    }

    private void setSecondaryButtonText(@StringRes int textId) {
        mSecondaryButton.setTitle(textId);
    }

    // Updates display message and proceed to next step according to the different text on
    // the primary button.
    private void handlePrimaryButtonClick() {
        // Need to check this because it can be fired from the keyboard.
        if (!shouldEnableSubmit()) {
            return;
        }

        mCurrentEntry = getEnteredPassword();

        switch (mUiStage) {
            case Introduction:
                mErrorCode = mPasswordHelper.validate(mCurrentEntry);
                if (mErrorCode == PasswordHelper.NO_ERROR) {
                    mFirstEntry = mCurrentEntry;
                    mPasswordField.setText("");
                    updateStage(Stage.NeedToConfirm);
                } else {
                    updateStage(Stage.PasswordInvalid);
                    mCurrentEntry.zeroize();
                }
                break;
            case NeedToConfirm:
            case SaveFailure:
                // Password must be entered twice. mFirstEntry is the one the user entered
                // the first time.  mCurrentEntry is what's currently in the input field
                if (Objects.equals(mFirstEntry, mCurrentEntry)) {
                    startSaveAndFinish();
                } else {
                    CharSequence tmp = mPasswordField.getText();
                    if (tmp != null) {
                        Selection.setSelection((Spannable) tmp, 0, tmp.length());
                    }
                    updateStage(Stage.ConfirmWrong);
                    mCurrentEntry.zeroize();
                }
                break;
            default:
                // Do nothing.
        }
    }

    // Updates display message and proceed to next step according to the different text on
    // the secondary button.
    private void handleSecondaryButtonClick() {
        if (mSaveLockWorker != null) {
            return;
        }

        if (mUiStage.secondaryButtonText == R.string.lockpassword_clear_label) {
            mPasswordField.setText("");
            mUiStage = Stage.Introduction;
            setSecondaryButtonText(mUiStage.secondaryButtonText);
        } else {
            getFragmentHost().goBack();
        }
    }

    @VisibleForTesting
    void onChosenLockSaveFinished(boolean isSaveSuccessful) {
        mProgressBar.setVisible(false);
        if (isSaveSuccessful) {
            onComplete();
        } else {
            updateStage(Stage.SaveFailure);
        }
    }

    // Starts an async task to save the chosen password.
    private void startSaveAndFinish() {
        if (mSaveLockWorker != null && !mSaveLockWorker.isFinished()) {
            LOG.v("startSaveAndFinish with a running SaveAndFinishWorker.");
            return;
        }

        mPasswordEntryInputDisabler.setInputEnabled(false);

        if (mSaveLockWorker == null) {
            mSaveLockWorker = new SaveLockWorker();
            mSaveLockWorker.setListener(this::onChosenLockSaveFinished);

            getFragmentManager()
                    .beginTransaction()
                    .add(mSaveLockWorker, FRAGMENT_TAG_SAVE_PASSWORD_WORKER)
                    .commitNow();
        }

        mSaveLockWorker.start(mUserId, mCurrentEntry, mExistingCredential);

        mProgressBar.setVisible(true);
        updateSubmitButtonsState();
    }

    // Updates the hint message, error, button text and state
    private void updateUi() {
        updateSubmitButtonsState();

        boolean inputAllowed = mSaveLockWorker == null || mSaveLockWorker.isFinished();

        if (mUiStage != Stage.Introduction) {
            setSecondaryButtonEnabled(inputAllowed);
        }

        if (mIsPin) {
            mPinPad.setEnterKeyIcon(mUiStage.enterKeyIcon);
        }

        switch (mUiStage) {
            case Introduction:
            case NeedToConfirm:
                mPasswordField.setError(null);
                mHintMessage.setText(getString(mUiStage.getHint(mIsAlphaMode)));
                break;
            case PasswordInvalid:
                List<String> messages =
                        mPasswordHelper.convertErrorCodeToMessages(getContext(), mErrorCode);
                setError(String.join(" ", messages));
                break;
            case ConfirmWrong:
            case SaveFailure:
                setError(getString(mUiStage.getHint(mIsAlphaMode)));
                break;
            default:
                // Do nothing
        }

        setPrimaryButtonText(mUiStage.primaryButtonText);
        setSecondaryButtonText(mUiStage.secondaryButtonText);
        mPasswordEntryInputDisabler.setInputEnabled(inputAllowed);
    }

    /**
     * To show error in password, it is set directly on TextInputEditText. PIN can't use
     * TextInputEditText because PIN field is not focusable therefore error won't show. Instead
     * the error is shown as a hint message.
     */
    private void setError(String message) {
        mHintMessage.setText(message);
    }

    @VisibleForTesting
    void updateStage(Stage stage) {
        mUiStage = stage;
        updateUi();
    }

    @VisibleForTesting
    void onComplete() {
        if (mCurrentEntry != null) {
            mCurrentEntry.zeroize();
        }

        if (mExistingCredential != null) {
            mExistingCredential.zeroize();
        }

        if (mFirstEntry != null) {
            mFirstEntry.zeroize();
        }

        mPasswordField.setText("");

        getActivity().finish();
    }

    // Keep track internally of where the user is in choosing a password.
    @VisibleForTesting
    enum Stage {
        Introduction(
                R.string.choose_lock_password_hints,
                R.string.choose_lock_pin_hints,
                R.string.continue_button_text,
                R.string.lockpassword_cancel_label,
                R.drawable.ic_arrow_forward),

        PasswordInvalid(
                R.string.lockpassword_invalid_password,
                R.string.lockpin_invalid_pin,
                R.string.continue_button_text,
                R.string.lockpassword_clear_label,
                R.drawable.ic_arrow_forward),

        NeedToConfirm(
                R.string.confirm_your_password_header,
                R.string.confirm_your_pin_header,
                R.string.lockpassword_confirm_label,
                R.string.lockpassword_cancel_label,
                R.drawable.ic_check),

        ConfirmWrong(
                R.string.confirm_passwords_dont_match,
                R.string.confirm_pins_dont_match,
                R.string.continue_button_text,
                R.string.lockpassword_cancel_label,
                R.drawable.ic_check),

        SaveFailure(
                R.string.error_saving_password,
                R.string.error_saving_lockpin,
                R.string.lockscreen_retry_button_text,
                R.string.lockpassword_cancel_label,
                R.drawable.ic_check);

        public final int alphaHint;
        public final int numericHint;
        public final int primaryButtonText;
        public final int secondaryButtonText;
        public final int enterKeyIcon;

        Stage(@StringRes int hintInAlpha,
                @StringRes int hintInNumeric,
                @StringRes int primaryButtonText,
                @StringRes int secondaryButtonText,
                @DrawableRes int enterKeyIcon) {
            this.alphaHint = hintInAlpha;
            this.numericHint = hintInNumeric;
            this.primaryButtonText = primaryButtonText;
            this.secondaryButtonText = secondaryButtonText;
            this.enterKeyIcon = enterKeyIcon;
        }

        @StringRes
        public int getHint(boolean isAlpha) {
            if (isAlpha) {
                return alphaHint;
            } else {
                return numericHint;
            }
        }
    }

    /**
     * Handler that batches text changed events
     */
    private class TextChangedHandler extends Handler {
        private static final int ON_TEXT_CHANGED = 1;
        private static final int DELAY_IN_MILLISECOND = 100;

        /**
         * With the introduction of delay, we batch processing the text changed event to reduce
         * unnecessary UI updates.
         */
        private void notifyAfterTextChanged() {
            removeMessages(ON_TEXT_CHANGED);
            sendEmptyMessageDelayed(ON_TEXT_CHANGED, DELAY_IN_MILLISECOND);
        }

        @Override
        public void handleMessage(Message msg) {
            if (msg.what == ON_TEXT_CHANGED) {
                mErrorCode = PasswordHelper.NO_ERROR;
                updateUi();
            }
        }
    }
}
