/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.customization.picker.theme;

import static android.app.Activity.RESULT_OK;

import static com.android.wallpaper.widget.BottomActionBar.BottomAction.APPLY;
import static com.android.wallpaper.widget.BottomActionBar.BottomAction.INFORMATION;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.customization.model.theme.DefaultThemeProvider;
import com.android.customization.model.theme.ThemeBundle;
import com.android.customization.model.theme.ThemeBundleProvider;
import com.android.customization.module.CustomizationInjector;
import com.android.customization.picker.WallpaperPreviewer;
import com.android.customization.widget.ThemeInfoView;
import com.android.wallpaper.R;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.picker.AppbarFragment;
import com.android.wallpaper.widget.BottomActionBar;

import com.bumptech.glide.Glide;

import org.json.JSONException;

/** A Fragment for theme full preview page. */
public class ThemeFullPreviewFragment extends AppbarFragment {
    private static final String TAG = "ThemeFullPreviewFragment";

    public static final String EXTRA_THEME_OPTION_TITLE = "theme_option_title";
    protected static final String EXTRA_THEME_OPTION = "theme_option";
    protected static final String EXTRA_WALLPAPER_INFO = "wallpaper_info";
    protected static final String EXTRA_CAN_APPLY_FROM_FULL_PREVIEW = "can_apply";

    private WallpaperInfo mWallpaper;
    private ThemeBundle mThemeBundle;
    private boolean mCanApplyFromFullPreview;

    /**
     * Returns a new {@link ThemeFullPreviewFragment} with the provided title and bundle arguments
     * set.
     */
    public static ThemeFullPreviewFragment newInstance(CharSequence title, Bundle intentBundle) {
        ThemeFullPreviewFragment fragment = new ThemeFullPreviewFragment();
        Bundle bundle = new Bundle();
        bundle.putAll(AppbarFragment.createArguments(title));
        bundle.putAll(intentBundle);
        fragment.setArguments(bundle);
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mWallpaper = getArguments().getParcelable(EXTRA_WALLPAPER_INFO);
        mCanApplyFromFullPreview = getArguments().getBoolean(EXTRA_CAN_APPLY_FROM_FULL_PREVIEW);
        CustomizationInjector injector = (CustomizationInjector) InjectorProvider.getInjector();
        ThemeBundleProvider themeProvider = new DefaultThemeProvider(
                getContext(), injector.getCustomizationPreferences(getContext()));
        try {
            ThemeBundle.Builder builder = themeProvider.parseThemeBundle(
                    getArguments().getString(EXTRA_THEME_OPTION));
            if (builder != null) {
                builder.setTitle(getArguments().getString(EXTRA_THEME_OPTION_TITLE));
                mThemeBundle = builder.build(getContext());
            }
        } catch (JSONException e) {
            Log.w(TAG, "Couldn't parse provided custom theme, will override it");
            // TODO(chihhangchuang): Handle the error case.
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(
                R.layout.fragment_theme_full_preview, container, /* attachToRoot */ false);
        setUpToolbar(view);
        Glide.get(getContext()).clearMemory();

        // Set theme option.
        final ThemeOptionPreviewer themeOptionPreviewer = new ThemeOptionPreviewer(
                getLifecycle(),
                getContext(),
                view.findViewById(R.id.theme_preview_container));
        themeOptionPreviewer.setPreviewInfo(mThemeBundle.getPreviewInfo());

        // Set wallpaper background.
        ImageView wallpaperImageView = view.findViewById(R.id.wallpaper_preview_image);
        final WallpaperPreviewer wallpaperPreviewer = new WallpaperPreviewer(
                getLifecycle(),
                getActivity(),
                wallpaperImageView,
                view.findViewById(R.id.wallpaper_preview_surface));
        wallpaperPreviewer.setWallpaper(mWallpaper,
                themeOptionPreviewer::updateColorForLauncherWidgets);
        return view;
    }

    @Override
    protected void onBottomActionBarReady(BottomActionBar bottomActionBar) {
        if (mCanApplyFromFullPreview) {
            bottomActionBar.showActionsOnly(INFORMATION, APPLY);
            bottomActionBar.setActionClickListener(APPLY, v -> finishActivityWithResultOk());
        } else {
            bottomActionBar.showActionsOnly(INFORMATION);
        }
        ThemeInfoView themeInfoView = (ThemeInfoView) LayoutInflater.from(getContext()).inflate(
                R.layout.theme_info_view, /* root= */ null);
        themeInfoView.populateThemeInfo(mThemeBundle);
        bottomActionBar.attachViewToBottomSheetAndBindAction(themeInfoView, INFORMATION);
        bottomActionBar.show();
    }

    private void finishActivityWithResultOk() {
        Activity activity = requireActivity();
        activity.overridePendingTransition(R.anim.fade_in, R.anim.fade_out);
        Intent intent = new Intent();
        activity.setResult(RESULT_OK, intent);
        activity.finish();
    }
}
