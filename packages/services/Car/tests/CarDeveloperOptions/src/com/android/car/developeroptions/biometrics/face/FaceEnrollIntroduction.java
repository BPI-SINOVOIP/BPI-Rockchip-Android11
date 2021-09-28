/*
 * Copyright (C) 2019 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.car.developeroptions.biometrics.face;

import android.app.admin.DevicePolicyManager;
import android.app.settings.SettingsEnums;
import android.content.ComponentName;
import android.content.Intent;
import android.hardware.face.FaceManager;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.View;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.TextView;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.Utils;
import com.android.car.developeroptions.biometrics.BiometricEnrollIntroduction;
import com.android.car.developeroptions.password.ChooseLockSettingsHelper;
import com.android.settingslib.RestrictedLockUtilsInternal;

import com.google.android.setupcompat.template.FooterBarMixin;
import com.google.android.setupcompat.template.FooterButton;
import com.google.android.setupcompat.util.WizardManagerHelper;
import com.google.android.setupdesign.span.LinkSpan;
import com.google.android.setupdesign.view.IllustrationVideoView;

public class FaceEnrollIntroduction extends BiometricEnrollIntroduction {

    private static final String TAG = "FaceIntro";

    private FaceManager mFaceManager;
    private FaceEnrollAccessibilityToggle mSwitchDiversity;

    private IllustrationVideoView mIllustrationNormal;
    private View mIllustrationAccessibility;

    private CompoundButton.OnCheckedChangeListener mSwitchDiversityListener =
            new CompoundButton.OnCheckedChangeListener() {
        @Override
        public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
            if (isChecked) {
                mIllustrationNormal.stop();
                mIllustrationNormal.setVisibility(View.INVISIBLE);
                mIllustrationAccessibility.setVisibility(View.VISIBLE);
            } else {
                mIllustrationNormal.setVisibility(View.VISIBLE);
                mIllustrationNormal.start();
                mIllustrationAccessibility.setVisibility(View.INVISIBLE);
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mFaceManager = Utils.getFaceManagerOrNull(this);
        final Button accessibilityButton = findViewById(R.id.accessibility_button);
        final View footerView = findViewById(R.id.footer_layout);
        accessibilityButton.setOnClickListener(view -> {
            mSwitchDiversity.setChecked(true);
            accessibilityButton.setVisibility(View.GONE);
            mSwitchDiversity.setVisibility(View.VISIBLE);
            footerView.setVisibility(View.GONE);
        });

        mSwitchDiversity = findViewById(R.id.toggle_diversity);
        mSwitchDiversity.setListener(mSwitchDiversityListener);

        mIllustrationNormal = findViewById(R.id.illustration_normal);
        mIllustrationAccessibility = findViewById(R.id.illustration_accessibility);

        mFooterBarMixin = getLayout().getMixin(FooterBarMixin.class);
        if (WizardManagerHelper.isAnySetupWizard(getIntent())) {
            mFooterBarMixin.setSecondaryButton(
                    new FooterButton.Builder(this)
                            .setText(R.string.skip_label)
                            .setListener(this::onSkipButtonClick)
                            .setButtonType(FooterButton.ButtonType.SKIP)
                            .setTheme(R.style.SudGlifButton_Secondary)
                            .build()
            );
        } else {
            mFooterBarMixin.setSecondaryButton(
                    new FooterButton.Builder(this)
                            .setText(R.string.security_settings_face_enroll_introduction_cancel)
                            .setListener(this::onCancelButtonClick)
                            .setButtonType(FooterButton.ButtonType.CANCEL)
                            .setTheme(R.style.SudGlifButton_Secondary)
                            .build()
            );
        }

        mFooterBarMixin.setPrimaryButton(
                new FooterButton.Builder(this)
                        .setText(R.string.wizard_next)
                        .setListener(this::onNextButtonClick)
                        .setButtonType(FooterButton.ButtonType.NEXT)
                        .setTheme(R.style.SudGlifButton_Primary)
                        .build()
        );
    }

    @Override
    protected void onResume() {
        super.onResume();
        mSwitchDiversityListener.onCheckedChanged(mSwitchDiversity.getSwitch(),
                mSwitchDiversity.isChecked());
     }

    @Override
    protected boolean isDisabledByAdmin() {
        return RestrictedLockUtilsInternal.checkIfKeyguardFeaturesDisabled(
                this, DevicePolicyManager.KEYGUARD_DISABLE_FACE, mUserId) != null;
    }

    @Override
    protected int getLayoutResource() {
        return R.layout.face_enroll_introduction;
    }

    @Override
    protected int getHeaderResDisabledByAdmin() {
        return R.string.security_settings_face_enroll_introduction_title_unlock_disabled;
    }

    @Override
    protected int getHeaderResDefault() {
        return R.string.security_settings_face_enroll_introduction_title;
    }

    @Override
    protected int getDescriptionResDisabledByAdmin() {
        return R.string.security_settings_face_enroll_introduction_message_unlock_disabled;
    }

    @Override
    protected FooterButton getCancelButton() {
        if (mFooterBarMixin != null) {
            return mFooterBarMixin.getSecondaryButton();
        }
        return null;
    }

    @Override
    protected FooterButton getNextButton() {
        if (mFooterBarMixin != null) {
            return mFooterBarMixin.getPrimaryButton();
        }
        return null;
    }

    @Override
    protected TextView getErrorTextView() {
        return findViewById(R.id.error_text);
    }

    @Override
    protected int checkMaxEnrolled() {
        if (mFaceManager != null) {
            final int max = getResources().getInteger(
                    com.android.internal.R.integer.config_faceMaxTemplatesPerUser);
            final int numEnrolledFaces = mFaceManager.getEnrolledFaces(mUserId).size();
            if (numEnrolledFaces >= max) {
                return R.string.face_intro_error_max;
            }
        } else {
            return R.string.face_intro_error_unknown;
        }
        return 0;
    }

    @Override
    protected long getChallenge() {
        mFaceManager = Utils.getFaceManagerOrNull(this);
        if (mFaceManager == null) {
            return 0;
        }
        return mFaceManager.generateChallenge();
    }

    @Override
    protected String getExtraKeyForBiometric() {
        return ChooseLockSettingsHelper.EXTRA_KEY_FOR_FACE;
    }

    @Override
    protected Intent getEnrollingIntent() {
        final String flattenedString = getString(R.string.config_face_enroll);
        final Intent intent = new Intent();
        if (!TextUtils.isEmpty(flattenedString)) {
            ComponentName componentName = ComponentName.unflattenFromString(flattenedString);
            intent.setComponent(componentName);

        } else {
            intent.setClass(this, FaceEnrollEnrolling.class);
        }
        intent.putExtra(EXTRA_KEY_REQUIRE_DIVERSITY, !mSwitchDiversity.isChecked());
        WizardManagerHelper.copyWizardManagerExtras(getIntent(), intent);
        return intent;
    }

    @Override
    protected int getConfirmLockTitleResId() {
        return R.string.security_settings_face_preference_title;
    }

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.FACE_ENROLL_INTRO;
    }

    @Override
    public void onClick(LinkSpan span) {
        // TODO(b/110906762)
    }
}
