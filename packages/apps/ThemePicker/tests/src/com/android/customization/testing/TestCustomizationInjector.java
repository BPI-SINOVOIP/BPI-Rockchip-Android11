package com.android.customization.testing;

import android.content.Context;

import androidx.fragment.app.FragmentActivity;

import com.android.customization.model.theme.OverlayManagerCompat;
import com.android.customization.model.theme.ThemeBundleProvider;
import com.android.customization.model.theme.ThemeManager;
import com.android.customization.module.CustomizationInjector;
import com.android.customization.module.CustomizationPreferences;
import com.android.customization.module.ThemesUserEventLogger;
import com.android.wallpaper.module.DrawableLayerResolver;
import com.android.wallpaper.module.PackageStatusNotifier;
import com.android.wallpaper.module.UserEventLogger;
import com.android.wallpaper.testing.TestInjector;

/**
 * Test implementation of the dependency injector.
 */
public class TestCustomizationInjector extends TestInjector implements CustomizationInjector {
    private CustomizationPreferences mCustomizationPreferences;
    private ThemeManager mThemeManager;
    private PackageStatusNotifier mPackageStatusNotifier;
    private DrawableLayerResolver mDrawableLayerResolver;
    private UserEventLogger mUserEventLogger;

    @Override
    public CustomizationPreferences getCustomizationPreferences(Context context) {
        if (mCustomizationPreferences == null) {
            mCustomizationPreferences = new TestDefaultCustomizationPreferences(context);
        }
        return mCustomizationPreferences;
    }

    @Override
    public ThemeManager getThemeManager(
            ThemeBundleProvider provider,
            FragmentActivity activity,
            OverlayManagerCompat overlayManagerCompat,
            ThemesUserEventLogger logger) {
        if (mThemeManager == null) {
            mThemeManager = new TestThemeManager(provider, activity, overlayManagerCompat, logger);
        }
        return mThemeManager;
    }

    @Override
    public PackageStatusNotifier getPackageStatusNotifier(Context context) {
        if (mPackageStatusNotifier == null) {
            mPackageStatusNotifier =  new TestPackageStatusNotifier();
        }
        return mPackageStatusNotifier;
    }

    @Override
    public DrawableLayerResolver getDrawableLayerResolver() {
        if (mDrawableLayerResolver == null) {
            mDrawableLayerResolver = new TestDrawableLayerResolver();
        }
        return mDrawableLayerResolver;
    }

    @Override
    public UserEventLogger getUserEventLogger(Context unused) {
        if (mUserEventLogger == null) {
            mUserEventLogger = new TestThemesUserEventLogger();
        }
        return mUserEventLogger;
    }
}
