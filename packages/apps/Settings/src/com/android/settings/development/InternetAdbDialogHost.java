package com.android.settings.development;

/**
 * Interface for EnableAdbWarningDialog callbacks.
 */
public interface InternetAdbDialogHost {
    /**
     * Called when the user presses enable on the warning dialog.
     */
    void onEnableInternetAdbDialogConfirmed();

    /**
     * Called when the user dismisses or cancels the warning dialog.
     */
    void onEnableInternetAdbDialogDismissed();
}