package com.android.customization.testing;

import androidx.fragment.app.FragmentActivity;

import com.android.customization.model.theme.OverlayManagerCompat;
import com.android.customization.model.theme.ThemeBundleProvider;
import com.android.customization.model.theme.ThemeManager;
import com.android.customization.module.ThemesUserEventLogger;

/**
 * Test implementation of {@link ThemeManager}.
 */
public class TestThemeManager extends ThemeManager {

    private static boolean sIsAvailable;

    public TestThemeManager(
            ThemeBundleProvider provider,
            FragmentActivity activity,
            OverlayManagerCompat overlayManagerCompat,
            ThemesUserEventLogger logger) {
        super(provider, activity, overlayManagerCompat, logger);
    }

    @Override
    public boolean isAvailable() {
        return sIsAvailable;
    }

    public static void setAvailable(boolean available) {
        sIsAvailable = available;
    }
}
