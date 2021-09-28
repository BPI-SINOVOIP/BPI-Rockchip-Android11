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
package com.android.customization.picker;

import static com.android.customization.picker.theme.ThemeFullPreviewFragment.EXTRA_THEME_OPTION_TITLE;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;

import com.android.customization.picker.grid.GridFullPreviewFragment;
import com.android.customization.picker.theme.ThemeFullPreviewFragment;
import com.android.wallpaper.R;
import com.android.wallpaper.widget.BottomActionBar;
import com.android.wallpaper.widget.BottomActionBar.BottomActionBarHost;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Activity for full preview. */
public class ViewOnlyFullPreviewActivity extends FragmentActivity implements BottomActionBarHost {

    private static final String EXTRA_PREVIEW_SECTION = "preview_section";
    private static final String EXTRA_PREVIEW_BUNDLE = "preview_bundle";

    public static final int SECTION_STYLE = 0;
    public static final int SECTION_GRID = 1;
    public static final int SECTION_CLOCK = 2;

    @IntDef({SECTION_STYLE, SECTION_GRID, SECTION_CLOCK})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Section {}

    /** Returns a new Intent with the provided data in the extra. */
    public static Intent newIntent(Context packageContext, @Section int section, Bundle bundle) {
        Intent intent = new Intent(packageContext, ViewOnlyFullPreviewActivity.class);
        intent.putExtra(EXTRA_PREVIEW_SECTION, section);
        intent.putExtra(EXTRA_PREVIEW_BUNDLE, bundle);
        return intent;
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_full_preview);

        final Intent intent = getIntent();
        @Section final int section = intent.getIntExtra(EXTRA_PREVIEW_SECTION, 0);
        final Bundle bundle = intent.getBundleExtra(EXTRA_PREVIEW_BUNDLE);
        if (section == SECTION_GRID) {
            showFragment(GridFullPreviewFragment.newInstance(
                    getString(R.string.grid_title), bundle));
        } else if (section == SECTION_STYLE) {
            final String themeTitle = bundle.getString(EXTRA_THEME_OPTION_TITLE);
            showFragment(ThemeFullPreviewFragment.newInstance(
                    TextUtils.isEmpty(themeTitle)
                            ? getString(R.string.theme_title)
                            : themeTitle,
                    bundle));
        }
    }

    @Override
    public BottomActionBar getBottomActionBar() {
        return findViewById(R.id.bottom_actionbar);
    }

    private void showFragment(Fragment fragment) {
        getSupportFragmentManager()
                .beginTransaction()
                .replace(R.id.preview_fragment_container, fragment)
                .commitNow();
    }
}
