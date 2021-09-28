/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.wallpaper.picker.individual;

import static com.android.wallpaper.picker.WallpaperPickerDelegate.PREVIEW_LIVE_WALLPAPER_REQUEST_CODE;
import static com.android.wallpaper.picker.WallpaperPickerDelegate.PREVIEW_WALLPAPER_REQUEST_CODE;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources.NotFoundException;
import android.os.Bundle;
import android.util.Log;
import android.view.MenuItem;
import android.widget.Toast;

import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import com.android.wallpaper.R;
import com.android.wallpaper.model.Category;
import com.android.wallpaper.model.CategoryProvider;
import com.android.wallpaper.model.CategoryReceiver;
import com.android.wallpaper.model.InlinePreviewIntentFactory;
import com.android.wallpaper.model.LiveWallpaperInfo;
import com.android.wallpaper.model.PickerIntentFactory;
import com.android.wallpaper.model.WallpaperCategory;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.Injector;
import com.android.wallpaper.module.InjectorProvider;
import com.android.wallpaper.module.WallpaperPersister;
import com.android.wallpaper.picker.BaseActivity;
import com.android.wallpaper.picker.PreviewActivity.PreviewActivityIntentFactory;
import com.android.wallpaper.util.DiskBasedLogger;
import com.android.wallpaper.widget.BottomActionBar;
import com.android.wallpaper.widget.BottomActionBar.BottomActionBarHost;

/**
 * Activity that can be launched from the Android wallpaper picker and allows users to pick from
 * various wallpapers and enter a preview mode for specific ones.
 */
public class IndividualPickerActivity extends BaseActivity implements BottomActionBarHost {
    private static final String TAG = "IndividualPickerAct";
    private static final String EXTRA_CATEGORY_COLLECTION_ID =
            "com.android.wallpaper.category_collection_id";
    private static final String EXTRA_WALLPAPER_ID = "com.android.wallpaper.wallpaper_id";
    private static final String KEY_CATEGORY_COLLECTION_ID = "key_category_collection_id";

    private InlinePreviewIntentFactory mPreviewIntentFactory;
    private WallpaperPersister mWallpaperPersister;
    private Category mCategory;
    private String mCategoryCollectionId;
    private String mWallpaperId;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mPreviewIntentFactory = new PreviewActivityIntentFactory();
        Injector injector = InjectorProvider.getInjector();
        mWallpaperPersister = injector.getWallpaperPersister(this);

        mCategoryCollectionId = (savedInstanceState == null)
                ? getIntent().getStringExtra(EXTRA_CATEGORY_COLLECTION_ID)
                : savedInstanceState.getString(KEY_CATEGORY_COLLECTION_ID);
        mWallpaperId = getIntent().getStringExtra(EXTRA_WALLPAPER_ID);

        if (mWallpaperId == null) { // Normal case
            initializeUI();
        } else { // Deeplink to preview page case
            setContentView(R.layout.activity_loading);
        }

        CategoryProvider categoryProvider = injector.getCategoryProvider(this);
        categoryProvider.fetchCategories(new CategoryReceiver() {
            @Override
            public void onCategoryReceived(Category category) {
                // Do nothing.
            }

            @Override
            public void doneFetchingCategories() {
                mCategory = categoryProvider.getCategory(mCategoryCollectionId);
                if (mCategory == null) {
                    DiskBasedLogger.e(TAG, "Failed to find the category: "
                            + mCategoryCollectionId, IndividualPickerActivity.this);
                    // We either were called with an invalid collection Id, or we're restarting
                    // with no saved state, or with a collection id that doesn't exist anymore.
                    // In those cases, we cannot continue, so let's just go back.
                    finish();
                    return;
                }

                // Show the preview of the specific wallpaper directly.
                if (mWallpaperId != null) {
                    ((WallpaperCategory) mCategory).fetchWallpapers(getApplicationContext(),
                            wallpapers -> {
                                for (WallpaperInfo wallpaper : wallpapers) {
                                    if (wallpaper.getWallpaperId().equals(mWallpaperId)) {
                                        showPreview(wallpaper);
                                        return;
                                    }
                                }
                                // No matched wallpaper, finish the activity.
                                finish();
                            }, false);
                } else {
                    onCategoryLoaded();
                }
            }
        }, false);
    }

    private void onCategoryLoaded() {
        setTitle(mCategory.getTitle());
        getSupportActionBar().setTitle(mCategory.getTitle());
    }

    private void initializeUI() {
        setContentView(R.layout.activity_individual_picker);

        // Set toolbar as the action bar.
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        FragmentManager fm = getSupportFragmentManager();
        Fragment fragment = fm.findFragmentById(R.id.fragment_container);

        getSupportActionBar().setDisplayHomeAsUpEnabled(true);

        toolbar.getNavigationIcon().setTint(getColor(R.color.toolbar_icon_color));
        toolbar.getNavigationIcon().setAutoMirrored(true);

        if (fragment == null) {
            fragment = InjectorProvider.getInjector()
                    .getIndividualPickerFragment(mCategoryCollectionId);
            fm.beginTransaction().add(R.id.fragment_container, fragment).commit();
        }
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == android.R.id.home) {
            // Handle Up as a Global back since the only entry point to IndividualPickerActivity is
            // from TopLevelPickerActivity.
            onBackPressed();
            return true;
        }
        return false;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        boolean shouldShowMessage = false;
        switch (requestCode) {
            case PREVIEW_LIVE_WALLPAPER_REQUEST_CODE:
                shouldShowMessage = true;
            case PREVIEW_WALLPAPER_REQUEST_CODE:
                if (resultCode == Activity.RESULT_OK) {
                    mWallpaperPersister.onLiveWallpaperSet();

                    // The wallpaper was set, so finish this activity with result OK.
                    finishWithResultOk(shouldShowMessage);
                }
                break;
            default:
                Log.e(TAG, "Invalid request code: " + requestCode);
        }

        // In deeplink to preview page case, this activity is just a middle layer
        // which redirects to preview activity. We should make sure this activity is finished
        // after getting the result in this case. Otherwise it will show an empty activity.
        if (mWallpaperId != null && !isFinishing()) {
            setResult(resultCode);
            finish();
        }
    }

    /**
     * Shows the preview activity for the given wallpaper.
     */
    public void showPreview(WallpaperInfo wallpaperInfo) {
        mWallpaperPersister.setWallpaperInfoInPreview(wallpaperInfo);
        wallpaperInfo.showPreview(this, mPreviewIntentFactory,
                wallpaperInfo instanceof LiveWallpaperInfo ? PREVIEW_LIVE_WALLPAPER_REQUEST_CODE
                        : PREVIEW_WALLPAPER_REQUEST_CODE);
    }

    private void finishWithResultOk(boolean shouldShowMessage) {
        if (shouldShowMessage) {
            try {
                Toast.makeText(this, R.string.wallpaper_set_successfully_message,
                        Toast.LENGTH_SHORT).show();
            } catch (NotFoundException e) {
                Log.e(TAG, "Could not show toast " + e);
            }
        }
        overridePendingTransition(R.anim.fade_in, R.anim.fade_out);
        setResult(Activity.RESULT_OK);
        finish();
    }

    @Override
    protected void onSaveInstanceState(Bundle bundle) {
        super.onSaveInstanceState(bundle);

        bundle.putString(KEY_CATEGORY_COLLECTION_ID, mCategoryCollectionId);
    }

    @Override
    public BottomActionBar getBottomActionBar() {
        return findViewById(R.id.bottom_actionbar);
    }

    /**
     * Default implementation of intent factory that provides an intent to start an
     * IndividualPickerActivity.
     */
    public static class IndividualPickerActivityIntentFactory implements PickerIntentFactory {
        @Override
        public Intent newIntent(Context ctx, String collectionId) {
            return new Intent(ctx, IndividualPickerActivity.class).putExtra(
                    EXTRA_CATEGORY_COLLECTION_ID, collectionId);
        }
    }
}
