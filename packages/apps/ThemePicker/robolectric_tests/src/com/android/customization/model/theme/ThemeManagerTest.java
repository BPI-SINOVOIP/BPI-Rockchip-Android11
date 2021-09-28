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
 * limitations under the License.
 */
package com.android.customization.model.theme;

import static com.android.customization.model.ResourceConstants.ANDROID_PACKAGE;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_COLOR;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_FONT;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_ANDROID;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_SETTINGS;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_SYSUI;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_SHAPE;
import static com.android.customization.model.ResourceConstants.SETTINGS_PACKAGE;
import static com.android.customization.model.ResourceConstants.SYSUI_PACKAGE;
import static com.android.customization.model.ResourceConstants.THEME_SETTING;

import static junit.framework.Assert.assertFalse;
import static junit.framework.Assert.assertTrue;
import static junit.framework.TestCase.assertEquals;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.provider.Settings;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

import com.android.customization.model.CustomizationManager.Callback;
import com.android.customization.model.CustomizationManager.OptionsFetchedListener;
import com.android.customization.model.theme.custom.CustomTheme;
import com.android.customization.module.ThemesUserEventLogger;
import com.android.customization.testutils.OverlayManagerMocks;

import org.json.JSONObject;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

@RunWith(RobolectricTestRunner.class)
public class ThemeManagerTest {

    @Mock OverlayManagerCompat mMockOm;
    @Mock ThemesUserEventLogger mThemesUserEventLogger;
    @Mock ThemeBundleProvider mThemeBundleProvider;
    private OverlayManagerMocks mMockOmHelper;
    private ThemeManager mThemeManager;
    private FragmentActivity mActivity;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        FragmentActivity activity = Robolectric.buildActivity(FragmentActivity.class).get();
        mActivity = spy(activity);
        mMockOmHelper = new OverlayManagerMocks();
        mMockOmHelper.setUpMock(mMockOm);
        mThemeManager = new ThemeManager(mThemeBundleProvider, activity,
                mMockOm, mThemesUserEventLogger);
    }

    @After
    public void cleanUp() {
        mMockOmHelper.clearOverlays();
    }

    @Test
    public void apply_WithDefaultTheme_StoresEmptyJsonString() {
        mMockOmHelper.addOverlay("test.package.name_color", ANDROID_PACKAGE,
                OVERLAY_CATEGORY_COLOR, true, 0);
        mMockOmHelper.addOverlay("test.package.name_font", ANDROID_PACKAGE,
                OVERLAY_CATEGORY_FONT, true, 0);
        mMockOmHelper.addOverlay("test.package.name_shape", ANDROID_PACKAGE,
                OVERLAY_CATEGORY_SHAPE, true, 0);
        mMockOmHelper.addOverlay("test.package.name_icon", ANDROID_PACKAGE,
                OVERLAY_CATEGORY_ICON_ANDROID, true, 0);
        mMockOmHelper.addOverlay("test.package.name_settings", SETTINGS_PACKAGE,
                OVERLAY_CATEGORY_ICON_SETTINGS, true, 0);
        mMockOmHelper.addOverlay("test.package.name_sysui", SYSUI_PACKAGE,
                OVERLAY_CATEGORY_ICON_SYSUI, true, 0);
        mMockOmHelper.addOverlay("test.package.name_themepicker", mActivity.getPackageName(),
                OVERLAY_CATEGORY_ICON_SYSUI, true, 0);

        ThemeBundle defaultTheme = new ThemeBundle.Builder().asDefault().build(mActivity);

        applyTheme(defaultTheme);

        assertEquals("Secure Setting should be empty JSON string after applying default theme",
                new JSONObject().toString(),
                Settings.Secure.getString(mActivity.getContentResolver(), THEME_SETTING));
    }

    @Test
    public void apply_WithOverlayTheme_StoresSerializedPackagesWithTimestamp() {
        ThemeBundle theme = getOverlayTheme();
        final String serializedPackagesWithTimestamp = theme.getSerializedPackagesWithTimestamp();

        theme = spy(theme);
        // Makes it return the fixed serializedPackagesWithTimestamp to test. Since we will get
        // fresh time every time, it's hard to compare for testing.
        when(theme.getSerializedPackagesWithTimestamp())
                .thenReturn(serializedPackagesWithTimestamp);

        applyTheme(theme);

        assertEquals("Secure Setting should be the overlay packages after applying theme",
                serializedPackagesWithTimestamp,
                Settings.Secure.getString(mActivity.getContentResolver(), THEME_SETTING));
    }

    @Test
    public void isAvailable_ThemeBundleProviderAndOverlayManagerAreAvailable_ReturnsTrue() {
        when(mThemeBundleProvider.isAvailable()).thenReturn(true);
        when(mMockOm.isAvailable()).thenReturn(true);

        assertTrue(mThemeManager.isAvailable());
    }

    @Test
    public void isAvailable_ThemeBundleProviderOrOverlayManagerIsAvailable_ReturnsFalse() {
        when(mThemeBundleProvider.isAvailable()).thenReturn(false);
        when(mMockOm.isAvailable()).thenReturn(true);
        assertFalse(mThemeManager.isAvailable());

        when(mThemeBundleProvider.isAvailable()).thenReturn(true);
        when(mMockOm.isAvailable()).thenReturn(false);
        assertFalse(mThemeManager.isAvailable());
    }

    @Test
    public void fetchOptions_ThemeBundleProviderFetches() {
        OptionsFetchedListener listener = mock(OptionsFetchedListener.class);

        mThemeManager.fetchOptions(listener, false);

        verify(mThemeBundleProvider).fetch(listener, false);
    }

    @Test
    public void removeCustomTheme_ThemeBundleProviderRemovesCustomTheme() {
        CustomTheme customTheme = mock(CustomTheme.class);
        mThemeManager.removeCustomTheme(customTheme);

        verify(mThemeBundleProvider).removeCustomTheme(customTheme);
    }

    @Test
    public void findThemeByPackages_ThemeBundleProviderFindsEquivalent() {
        CustomTheme theme = mock(CustomTheme.class);
        mThemeManager.findThemeByPackages(theme);

        verify(mThemeBundleProvider).findEquivalent(theme);
    }

    @Test
    public void storeEmptyTheme_SettingsSecureStoresEmptyTheme() {
        mThemeManager.storeEmptyTheme();

        assertEquals(
                new JSONObject().toString(),
                Settings.Secure.getString(mActivity.getContentResolver(), THEME_SETTING));
    }

    @Test
    public void getStoredOverlays_GetsFromSettingsSecureWithExpectedName() {
        ThemeBundle theme = getOverlayTheme();

        applyTheme(theme);

        assertEquals(
                Settings.Secure.getString(mActivity.getContentResolver(), THEME_SETTING),
                mThemeManager.getStoredOverlays());
    }

    private ThemeBundle getOverlayTheme() {
        final String bundleColorPackage = "test.package.name_color";
        final String bundleFontPackage = "test.package.name_font";
        final String otherPackage = "other.package.name_font";

        mMockOmHelper.addOverlay(bundleColorPackage, ANDROID_PACKAGE,
                OVERLAY_CATEGORY_COLOR, false, 0);
        mMockOmHelper.addOverlay(bundleFontPackage, ANDROID_PACKAGE,
                OVERLAY_CATEGORY_FONT, false, 0);
        mMockOmHelper.addOverlay(otherPackage, ANDROID_PACKAGE,
                OVERLAY_CATEGORY_FONT, false, 0);

        return new ThemeBundle.Builder()
                .addOverlayPackage(OVERLAY_CATEGORY_COLOR, bundleColorPackage)
                .addOverlayPackage(OVERLAY_CATEGORY_FONT, bundleFontPackage)
                .build(mActivity);
    }

    private void applyTheme(ThemeBundle theme) {
        mThemeManager.apply(theme, new Callback() {
            @Override
            public void onSuccess() {
            }

            @Override
            public void onError(@Nullable Throwable throwable) {
            }
        });
    }
}
