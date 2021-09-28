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
package com.android.customization.picker.theme;

import static android.app.Activity.RESULT_OK;

import static com.android.customization.picker.ViewOnlyFullPreviewActivity.SECTION_STYLE;
import static com.android.customization.picker.theme.ThemeFullPreviewFragment.EXTRA_CAN_APPLY_FROM_FULL_PREVIEW;
import static com.android.customization.picker.theme.ThemeFullPreviewFragment.EXTRA_THEME_OPTION;
import static com.android.customization.picker.theme.ThemeFullPreviewFragment.EXTRA_THEME_OPTION_TITLE;
import static com.android.customization.picker.theme.ThemeFullPreviewFragment.EXTRA_WALLPAPER_INFO;
import static com.android.wallpaper.widget.BottomActionBar.BottomAction.APPLY;
import static com.android.wallpaper.widget.BottomActionBar.BottomAction.CUSTOMIZE;
import static com.android.wallpaper.widget.BottomActionBar.BottomAction.INFORMATION;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.widget.ContentLoadingProgressBar;
import androidx.recyclerview.widget.RecyclerView;

import com.android.customization.model.CustomizationManager.Callback;
import com.android.customization.model.CustomizationManager.OptionsFetchedListener;
import com.android.customization.model.CustomizationOption;
import com.android.customization.model.theme.ThemeBundle;
import com.android.customization.model.theme.ThemeManager;
import com.android.customization.model.theme.custom.CustomTheme;
import com.android.customization.module.ThemesUserEventLogger;
import com.android.customization.picker.ViewOnlyFullPreviewActivity;
import com.android.customization.picker.WallpaperPreviewer;
import com.android.customization.widget.OptionSelectorController;
import com.android.customization.widget.ThemeInfoView;
import com.android.wallpaper.R;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.CurrentWallpaperInfoFactory;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.picker.AppbarFragment;
import com.android.wallpaper.widget.BottomActionBar;
import com.android.wallpaper.widget.BottomActionBar.AccessibilityCallback;

import java.util.List;

/**
 * Fragment that contains the main UI for selecting and applying a ThemeBundle.
 */
public class ThemeFragment extends AppbarFragment {

    private static final String TAG = "ThemeFragment";
    private static final String KEY_SELECTED_THEME = "ThemeFragment.SelectedThemeBundle";
    private static final String KEY_STATE_BOTTOM_ACTION_BAR_VISIBLE =
            "ThemeFragment.bottomActionBarVisible";
    private static final int FULL_PREVIEW_REQUEST_CODE = 1000;

    /**
     * Interface to be implemented by an Activity hosting a {@link ThemeFragment}
     */
    public interface ThemeFragmentHost {
        ThemeManager getThemeManager();
    }
    public static ThemeFragment newInstance(CharSequence title) {
        ThemeFragment fragment = new ThemeFragment();
        fragment.setArguments(AppbarFragment.createArguments(title));
        return fragment;
    }

    private RecyclerView mOptionsContainer;
    private OptionSelectorController<ThemeBundle> mOptionsController;
    private ThemeManager mThemeManager;
    private ThemesUserEventLogger mEventLogger;
    private ThemeBundle mSelectedTheme;
    private ContentLoadingProgressBar mLoading;
    private View mContent;
    private View mError;
    private WallpaperInfo mCurrentHomeWallpaper;
    private CurrentWallpaperInfoFactory mCurrentWallpaperFactory;
    private BottomActionBar mBottomActionBar;
    private WallpaperPreviewer mWallpaperPreviewer;
    private ImageView mWallpaperImage;
    private ThemeOptionPreviewer mThemeOptionPreviewer;
    private ThemeInfoView mThemeInfoView;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mThemeManager = ((ThemeFragmentHost) context).getThemeManager();
        mEventLogger = (ThemesUserEventLogger)
                InjectorProvider.getInjector().getUserEventLogger(context);
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(
                R.layout.fragment_theme_picker, container, /* attachToRoot */ false);
        setUpToolbar(view);

        mContent = view.findViewById(R.id.content_section);
        mLoading = view.findViewById(R.id.loading_indicator);
        mError = view.findViewById(R.id.error_section);
        mCurrentWallpaperFactory = InjectorProvider.getInjector()
                .getCurrentWallpaperFactory(getActivity().getApplicationContext());
        mOptionsContainer = view.findViewById(R.id.options_container);

        mThemeOptionPreviewer = new ThemeOptionPreviewer(
                getLifecycle(),
                getContext(),
                view.findViewById(R.id.theme_preview_container));

        // Set Wallpaper background.
        mWallpaperImage = view.findViewById(R.id.wallpaper_preview_image);
        mWallpaperPreviewer = new WallpaperPreviewer(
                getLifecycle(),
                getActivity(),
                mWallpaperImage,
                view.findViewById(R.id.wallpaper_preview_surface));
        mCurrentWallpaperFactory.createCurrentWallpaperInfos(
                (homeWallpaper, lockWallpaper, presentationMode) -> {
                    mCurrentHomeWallpaper = homeWallpaper;
                    mWallpaperPreviewer.setWallpaper(mCurrentHomeWallpaper,
                            mThemeOptionPreviewer::updateColorForLauncherWidgets);
                }, false);

        view.findViewById(R.id.theme_preview_card).setOnClickListener(v -> showFullPreview());
        return view;
    }

    @Override
    protected void onBottomActionBarReady(BottomActionBar bottomActionBar) {
        mBottomActionBar = bottomActionBar;
        mBottomActionBar.showActionsOnly(INFORMATION, APPLY);
        mBottomActionBar.setActionClickListener(APPLY, v -> {
            mBottomActionBar.disableActions();
            applyTheme();
        });
        mThemeInfoView = (ThemeInfoView) LayoutInflater.from(getContext()).inflate(
                R.layout.theme_info_view, /* root= */ null);
        mBottomActionBar.attachViewToBottomSheetAndBindAction(mThemeInfoView, INFORMATION);
        mBottomActionBar.setActionClickListener(CUSTOMIZE, this::onCustomizeClicked);

        // Update target view's accessibility param since it will be blocked by the bottom sheet
        // when expanded.
        mBottomActionBar.setAccessibilityCallback(new AccessibilityCallback() {
            @Override
            public void onBottomSheetCollapsed() {
                mOptionsContainer.setImportantForAccessibility(
                        View.IMPORTANT_FOR_ACCESSIBILITY_YES);
            }

            @Override
            public void onBottomSheetExpanded() {
                mOptionsContainer.setImportantForAccessibility(
                        View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
            }
        });
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        // Setup options here when all views are ready(including BottomActionBar), since we need to
        // update views after options are loaded.
        setUpOptions(savedInstanceState);
    }

    private void applyTheme() {
        mThemeManager.apply(mSelectedTheme, new Callback() {
            @Override
            public void onSuccess() {
                Toast.makeText(getContext(), R.string.applied_theme_msg, Toast.LENGTH_LONG).show();
                getActivity().overridePendingTransition(R.anim.fade_in, R.anim.fade_out);
                getActivity().finish();
            }

            @Override
            public void onError(@Nullable Throwable throwable) {
                Log.w(TAG, "Error applying theme", throwable);
                // Since we disabled it when clicked apply button.
                mBottomActionBar.enableActions();
                mBottomActionBar.hide();
                Toast.makeText(getContext(), R.string.apply_theme_error_msg,
                        Toast.LENGTH_LONG).show();
            }
        });
    }

    @Override
    public void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        if (mSelectedTheme != null && !mSelectedTheme.isActive(mThemeManager)) {
            outState.putString(KEY_SELECTED_THEME, mSelectedTheme.getSerializedPackages());
        }
        if (mBottomActionBar != null) {
            outState.putBoolean(KEY_STATE_BOTTOM_ACTION_BAR_VISIBLE, mBottomActionBar.isVisible());
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        if (requestCode == CustomThemeActivity.REQUEST_CODE_CUSTOM_THEME) {
            if (resultCode == CustomThemeActivity.RESULT_THEME_DELETED) {
                mSelectedTheme = null;
                reloadOptions();
            } else if (resultCode == CustomThemeActivity.RESULT_THEME_APPLIED) {
                getActivity().overridePendingTransition(R.anim.fade_in, R.anim.fade_out);
                getActivity().finish();
            } else {
                if (mSelectedTheme != null) {
                    mOptionsController.setSelectedOption(mSelectedTheme);
                    // Set selected option above will show BottomActionBar,
                    // hide BottomActionBar for the mis-trigger.
                    mBottomActionBar.hide();
                } else {
                    reloadOptions();
                }
            }
        } else if (requestCode == FULL_PREVIEW_REQUEST_CODE && resultCode == RESULT_OK) {
            applyTheme();
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    private void onCustomizeClicked(View view) {
        if (mSelectedTheme instanceof CustomTheme) {
            navigateToCustomTheme((CustomTheme) mSelectedTheme);
        }
    }

    private void hideError() {
        mContent.setVisibility(View.VISIBLE);
        mError.setVisibility(View.GONE);
    }

    private void showError() {
        mLoading.hide();
        mContent.setVisibility(View.GONE);
        mError.setVisibility(View.VISIBLE);
    }

    private void setUpOptions(@Nullable Bundle savedInstanceState) {
        hideError();
        mLoading.show();
        mThemeManager.fetchOptions(new OptionsFetchedListener<ThemeBundle>() {
            @Override
            public void onOptionsLoaded(List<ThemeBundle> options) {
                mOptionsController = new OptionSelectorController<>(mOptionsContainer, options);
                mOptionsController.initOptions(mThemeManager);

                // Find out the selected theme option.
                // 1. Find previously selected theme.
                String previouslySelected = savedInstanceState != null
                        ? savedInstanceState.getString(KEY_SELECTED_THEME) : null;
                ThemeBundle previouslySelectedTheme = null;
                ThemeBundle activeTheme = null;
                for (ThemeBundle theme : options) {
                    if (previouslySelected != null
                            && previouslySelected.equals(theme.getSerializedPackages())) {
                        previouslySelectedTheme = theme;
                    }
                    if (theme.isActive(mThemeManager)) {
                        activeTheme = theme;
                    }
                }
                // 2. Use active theme if no previously selected theme.
                mSelectedTheme = previouslySelectedTheme != null
                        ? previouslySelectedTheme
                        : activeTheme;
                // 3. Select the first system theme(default theme currently)
                //    if there is no matching custom enabled theme.
                if (mSelectedTheme == null) {
                    mSelectedTheme = findFirstSystemThemeBundle(options);
                }

                mOptionsController.setSelectedOption(mSelectedTheme);
                onOptionSelected(mSelectedTheme);
                restoreBottomActionBarVisibility(savedInstanceState);

                mOptionsController.addListener(selectedOption -> {
                    onOptionSelected(selectedOption);
                    if (!isAddCustomThemeOption(selectedOption)) {
                        mBottomActionBar.show();
                    }
                });
                mLoading.hide();
            }
            @Override
            public void onError(@Nullable Throwable throwable) {
                if (throwable != null) {
                    Log.e(TAG, "Error loading theme bundles", throwable);
                }
                showError();
            }
        }, false);
    }

    private void reloadOptions() {
        mThemeManager.fetchOptions(options -> {
            mOptionsController.resetOptions(options);
            for (ThemeBundle theme : options) {
                if (theme.isActive(mThemeManager)) {
                    mSelectedTheme = theme;
                    break;
                }
            }
            if (mSelectedTheme == null) {
                mSelectedTheme = findFirstSystemThemeBundle(options);
            }
            mOptionsController.setSelectedOption(mSelectedTheme);
            // Set selected option above will show BottomActionBar,
            // hide BottomActionBar for the mis-trigger.
            mBottomActionBar.hide();
        }, true);
    }

    private ThemeBundle findFirstSystemThemeBundle(List<ThemeBundle> options) {
        for (ThemeBundle bundle : options) {
            if (!(bundle instanceof CustomTheme)) {
                return bundle;
            }
        }
        return null;
    }

    private void onOptionSelected(CustomizationOption selectedOption) {
        if (isAddCustomThemeOption(selectedOption)) {
            navigateToCustomTheme((CustomTheme) selectedOption);
        } else {
            mSelectedTheme = (ThemeBundle) selectedOption;
            mSelectedTheme.setOverrideThemeWallpaper(mCurrentHomeWallpaper);
            mEventLogger.logThemeSelected(mSelectedTheme,
                    selectedOption instanceof CustomTheme);
            mThemeOptionPreviewer.setPreviewInfo(mSelectedTheme.getPreviewInfo());
            if (mThemeInfoView != null && mSelectedTheme != null) {
                mThemeInfoView.populateThemeInfo(mSelectedTheme);
            }

            if (selectedOption instanceof CustomTheme) {
                mBottomActionBar.showActionsOnly(INFORMATION, CUSTOMIZE, APPLY);
            } else {
                mBottomActionBar.showActionsOnly(INFORMATION, APPLY);
            }
        }
    }

    private void restoreBottomActionBarVisibility(@Nullable Bundle savedInstanceState) {
        boolean isBottomActionBarVisible = savedInstanceState != null
                && savedInstanceState.getBoolean(KEY_STATE_BOTTOM_ACTION_BAR_VISIBLE);
        if (isBottomActionBarVisible) {
            mBottomActionBar.show();
        } else {
            mBottomActionBar.hide();
        }
    }

    private boolean isAddCustomThemeOption(CustomizationOption option) {
        return option instanceof CustomTheme && !((CustomTheme) option).isDefined();
    }

    private void navigateToCustomTheme(CustomTheme themeToEdit) {
        Intent intent = new Intent(getActivity(), CustomThemeActivity.class);
        intent.putExtra(CustomThemeActivity.EXTRA_THEME_TITLE, themeToEdit.getTitle());
        intent.putExtra(CustomThemeActivity.EXTRA_THEME_ID, themeToEdit.getId());
        intent.putExtra(CustomThemeActivity.EXTRA_THEME_PACKAGES,
                themeToEdit.getSerializedPackages());
        startActivityForResult(intent, CustomThemeActivity.REQUEST_CODE_CUSTOM_THEME);
    }

    private void showFullPreview() {
        Bundle bundle = new Bundle();
        bundle.putParcelable(EXTRA_WALLPAPER_INFO, mCurrentHomeWallpaper);
        bundle.putString(EXTRA_THEME_OPTION, mSelectedTheme.getSerializedPackages());
        bundle.putString(EXTRA_THEME_OPTION_TITLE, mSelectedTheme.getTitle());
        bundle.putBoolean(EXTRA_CAN_APPLY_FROM_FULL_PREVIEW, true);
        Intent intent = ViewOnlyFullPreviewActivity.newIntent(getContext(), SECTION_STYLE, bundle);
        startActivityForResult(intent, FULL_PREVIEW_REQUEST_CODE);
    }
}
