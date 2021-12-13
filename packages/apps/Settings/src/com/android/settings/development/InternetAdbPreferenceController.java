package com.android.settings.development;


import android.content.Context;
import androidx.annotation.Nullable;
import androidx.preference.Preference;

import com.android.settings.core.PreferenceControllerMixin;
import com.android.settingslib.development.AbstractEnableInternetAdbPreferenceController;

public class InternetAdbPreferenceController extends AbstractEnableInternetAdbPreferenceController implements
        PreferenceControllerMixin {

    private final DevelopmentSettingsDashboardFragment mFragment;

    public InternetAdbPreferenceController(Context context, DevelopmentSettingsDashboardFragment fragment) {
        super(context);
        mFragment = fragment;
    }

    public void onInternetAdbDialogConfirmed() {
        writeInternetAdbSetting(true);
    }

    public void onInternetAdbDialogDismissed() {
        updateState(mPreference);
    }

    @Override
    public void showConfirmationDialog(@Nullable Preference preference) {
        EnableInternetAdbWarningDialog.show(mFragment);
    }

    @Override
    public void dismissConfirmationDialog() {
        // intentional no-op
    }

    @Override
    public boolean isConfirmationDialogShowing() {
        // intentional no-op
        return false;
    }

    @Override
    protected void onDeveloperOptionsSwitchDisabled() {
        super.onDeveloperOptionsSwitchDisabled();
        writeInternetAdbSetting(false);
        mPreference.setChecked(false);
    }
}