/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.tv.settings.connectivity.setup;

import android.content.Context;
import android.os.Bundle;
import android.text.InputType;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.EditText;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.leanback.widget.GuidanceStylist;
import androidx.leanback.widget.GuidedAction;
import androidx.leanback.widget.GuidedActionsStylist;
import androidx.lifecycle.ViewModelProviders;

import com.android.tv.settings.R;
import com.android.tv.settings.connectivity.security.WifiSecurityHelper;
import com.android.tv.settings.connectivity.util.GuidedActionsAlignUtil;
import com.android.tv.settings.connectivity.util.State;
import com.android.tv.settings.connectivity.util.StateMachine;

import java.util.List;

/**
 * State responsible for entering the password.
 */
public class EnterPasswordState implements State {
    private final FragmentActivity mActivity;
    private Fragment mFragment;
    private static final int ACTION_ID_CHECKBOX = 999;
    private UserChoiceInfo mUserChoiceInfo;
    private StateMachine mStateMachine;

    public EnterPasswordState(FragmentActivity activity) {
        mActivity = activity;
        mUserChoiceInfo = ViewModelProviders.of(mActivity).get(UserChoiceInfo.class);
        mStateMachine = ViewModelProviders.of(mActivity).get(StateMachine.class);
    }

    @Override
    public void processForward() {
        if (!mUserChoiceInfo.isVisible(UserChoiceInfo.PASSWORD)) {
            mStateMachine.getListener().onComplete(StateMachine.OPTIONS_OR_CONNECT);
            return;
        }
        mFragment = new EnterPasswordFragment();
        FragmentChangeListener listener = (FragmentChangeListener) mActivity;
        listener.onFragmentChange(mFragment, true);
    }

    @Override
    public void processBackward() {
        if (!mUserChoiceInfo.isVisible(UserChoiceInfo.PASSWORD)) {
            mStateMachine.getListener().onComplete(StateMachine.CANCEL);
            return;
        }
        mFragment = new EnterPasswordFragment();
        FragmentChangeListener listener = (FragmentChangeListener) mActivity;
        listener.onFragmentChange(mFragment, false);
    }

    @Override
    public Fragment getFragment() {
        return mFragment;
    }

    /**
     * Fragment that enters password of the Wi-Fi.
     */
    public static class EnterPasswordFragment extends WifiConnectivityGuidedStepFragment {
        private static final int PSK_MIN_LENGTH = 8;
        private static final int WEP_MIN_LENGTH = 5;
        private UserChoiceInfo mUserChoiceInfo;
        private StateMachine mStateMachine;
        private EditText mTextInput;
        private CheckBox mCheckBox;
        private GuidedAction mPasswordAction;
        private boolean mEditFocused = false;

        @NonNull
        @Override
        public GuidanceStylist.Guidance onCreateGuidance(Bundle savedInstanceState) {
            String title = getString(
                    R.string.wifi_setup_input_password,
                    WifiSecurityHelper.getSsid(getActivity()));
            return new GuidanceStylist.Guidance(
                    title,
                    null,
                    null,
                    null);
        }

        @Override
        public GuidedActionsStylist onCreateActionsStylist() {
            return new GuidedActionsStylist() {
                @Override
                public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
                    LayoutInflater inflater = LayoutInflater.from(parent.getContext());
                    View v = inflater.inflate(onProvideItemLayoutId(viewType), parent, false);
                    if (viewType == ACTION_ID_CHECKBOX) {
                        return new PasswordViewHolder(v);
                    }
                    return new GuidedActionsAlignUtil.SetupViewHolder(v);
                }

                @Override
                public int getItemViewType(GuidedAction action) {
                    return (int) action.getId();
                }

                @Override
                public void onBindViewHolder(ViewHolder vh, GuidedAction action) {
                    super.onBindViewHolder(vh, action);
                    if (action.getId() == ACTION_ID_CHECKBOX) {
                        PasswordViewHolder checkBoxVH = (PasswordViewHolder) vh;
                        mCheckBox = checkBoxVH.mCheckbox;
                        mCheckBox.setOnCheckedChangeListener((view, isChecked) -> {
                            if (mPasswordAction != null) {
                                setSelectedActionPosition(0);
                            }
                            mTextInput.setInputType(InputType.TYPE_CLASS_TEXT | (isChecked ? InputType.TYPE_TEXT_VARIATION_PASSWORD : InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD));
                        });
                        checkBoxVH.itemView.setOnClickListener(view -> {
                            mCheckBox.setChecked(!mCheckBox.isChecked());
                            if (mPasswordAction != null) {
                                setSelectedActionPosition(0);
                            }
                        });
                        mCheckBox.setChecked(mUserChoiceInfo.isPasswordHidden());
                    } else if (action.getId() == GuidedAction.ACTION_ID_CONTINUE) {
                        mTextInput = (EditText) vh.itemView.findViewById(
                                R.id.guidedactions_item_title);
                        openInEditMode(action);
                    }
                }

                @Override
                protected void onEditingModeChange(ViewHolder vh, boolean editing,
                        boolean withTransition) {
                    super.onEditingModeChange(vh, editing, withTransition);
                    updatePasswordInputObfuscation();
                }

                @Override
                public int onProvideItemLayoutId(int viewType) {
                    if (viewType == ACTION_ID_CHECKBOX) {
                        return R.layout.password_checkbox;
                    } else {
                        return R.layout.setup_password_item;
                    }
                }
            };
        }

        @Override
        public void onCreate(Bundle savedInstanceState) {
            mUserChoiceInfo = ViewModelProviders
                    .of(getActivity())
                    .get(UserChoiceInfo.class);
            mStateMachine = ViewModelProviders
                    .of(getActivity())
                    .get(StateMachine.class);
            super.onCreate(savedInstanceState);
        }

        @Override
        public void onCreateActions(@NonNull List<GuidedAction> actions,
                Bundle savedInstanceState) {
            Context context = getActivity();
            CharSequence prevPassword = mUserChoiceInfo.getPageSummary(UserChoiceInfo.PASSWORD);
            boolean isPasswordHidden = mUserChoiceInfo.isPasswordHidden();
            mPasswordAction = new GuidedAction.Builder(context)
                    .title(prevPassword == null ? "" : prevPassword)
                    .editInputType(InputType.TYPE_CLASS_TEXT
                            | (isPasswordHidden
                            ? InputType.TYPE_TEXT_VARIATION_PASSWORD
                            : InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD))
                    .id(GuidedAction.ACTION_ID_CONTINUE)
                    .editable(true)
                    .build();
            actions.add(mPasswordAction);
            GuidedAction checkboxAction = new GuidedAction.Builder(context)
                    .id(ACTION_ID_CHECKBOX)
                    .build();
            actions.add(checkboxAction);
        }

        @Override
        public long onGuidedActionEditedAndProceed(GuidedAction action) {
            if (action.getId() == GuidedAction.ACTION_ID_CONTINUE) {
                String password = action.getTitle().toString();
                if (password.length() >= WEP_MIN_LENGTH) {
                    mUserChoiceInfo.put(UserChoiceInfo.PASSWORD, action.getTitle().toString());
                    mUserChoiceInfo.setPasswordHidden(mCheckBox.isChecked());
                    mStateMachine.getListener().onComplete(StateMachine.OPTIONS_OR_CONNECT);
                }
            }
            return action.getId();
        }

        @Override
        public void onGuidedActionFocused(GuidedAction action) {
            boolean newEditFocused = action == mPasswordAction;
            if (!mEditFocused && newEditFocused) {
                openInEditMode(action);
            }
            mEditFocused = newEditFocused;
        }

        private void updatePasswordInputObfuscation() {
            mTextInput.setInputType(InputType.TYPE_CLASS_TEXT
                    | (mCheckBox.isChecked()
                    ? InputType.TYPE_TEXT_VARIATION_PASSWORD
                    : InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD));
        }


        private static class PasswordViewHolder extends GuidedActionsAlignUtil.SetupViewHolder {
            CheckBox mCheckbox;

            PasswordViewHolder(View v) {
                super(v);
                mCheckbox = v.findViewById(R.id.password_checkbox);
            }
        }
    }
}
