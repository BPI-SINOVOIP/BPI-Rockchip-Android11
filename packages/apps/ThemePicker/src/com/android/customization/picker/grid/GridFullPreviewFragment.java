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
package com.android.customization.picker.grid;

import static android.app.Activity.RESULT_OK;

import static com.android.wallpaper.widget.BottomActionBar.BottomAction.APPLY;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.customization.model.grid.GridOption;
import com.android.customization.model.grid.GridOptionsManager;
import com.android.customization.model.grid.LauncherGridOptionsProvider;
import com.android.customization.module.CustomizationInjector;
import com.android.customization.module.ThemesUserEventLogger;
import com.android.customization.picker.WallpaperPreviewer;
import com.android.wallpaper.R;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.picker.AppbarFragment;
import com.android.wallpaper.widget.BottomActionBar;

import com.bumptech.glide.Glide;


/** A Fragment for grid full preview page. */
public class GridFullPreviewFragment extends AppbarFragment {

    static final String EXTRA_WALLPAPER_INFO = "wallpaper_info";
    static final String EXTRA_GRID_OPTION = "grid_option";

    private WallpaperInfo mWallpaper;
    private GridOption mGridOption;

    private WallpaperPreviewer mWallpaperPreviewer;
    private GridOptionPreviewer mGridOptionPreviewer;

    /**
     * Returns a new {@link GridFullPreviewFragment} with the provided title and bundle arguments
     * set.
     */
    public static GridFullPreviewFragment newInstance(CharSequence title, Bundle intentBundle) {
        GridFullPreviewFragment fragment = new GridFullPreviewFragment();
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
        mGridOption = getArguments().getParcelable(EXTRA_GRID_OPTION);
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(
                R.layout.fragment_grid_full_preview, container, /* attachToRoot */ false);
        setUpToolbar(view);

        // Clear memory cache whenever grid fragment view is being loaded.
        Glide.get(getContext()).clearMemory();

        ImageView wallpaperPreviewImage = view.findViewById(R.id.wallpaper_preview_image);
        SurfaceView wallpaperSurface = view.findViewById(R.id.wallpaper_preview_surface);
        mWallpaperPreviewer = new WallpaperPreviewer(
                getLifecycle(), getActivity(), wallpaperPreviewImage, wallpaperSurface);

        CustomizationInjector injector = (CustomizationInjector) InjectorProvider.getInjector();
        ThemesUserEventLogger eventLogger = (ThemesUserEventLogger) injector.getUserEventLogger(
                getContext());
        final GridOptionsManager gridManager = new GridOptionsManager(
                new LauncherGridOptionsProvider(getContext(),
                        getString(R.string.grid_control_metadata_name)),
                eventLogger);

        mGridOptionPreviewer = new GridOptionPreviewer(gridManager,
                view.findViewById(R.id.grid_preview_container));
        return view;
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        mWallpaperPreviewer.setWallpaper(mWallpaper, /* listener= */ null);
        mGridOptionPreviewer.setGridOption(mGridOption);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        if (mGridOptionPreviewer != null) {
            mGridOptionPreviewer.release();
        }
    }

    @Override
    protected void onBottomActionBarReady(BottomActionBar bottomActionBar) {
        bottomActionBar.showActionsOnly(APPLY);
        bottomActionBar.setActionClickListener(APPLY, v -> finishActivityWithResultOk());
        bottomActionBar.show();
    }

    private void finishActivityWithResultOk() {
        Activity activity = requireActivity();
        activity.overridePendingTransition(R.anim.fade_in, R.anim.fade_out);
        Intent intent = new Intent();
        intent.putExtra(EXTRA_GRID_OPTION, mGridOption);
        activity.setResult(RESULT_OK, intent);
        activity.finish();
    }
}
