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
package com.android.wallpaper.testing;

import android.os.Handler;

import com.android.wallpaper.model.Category;
import com.android.wallpaper.model.CategoryProvider;
import com.android.wallpaper.model.CategoryReceiver;
import com.android.wallpaper.model.ImageCategory;
import com.android.wallpaper.model.WallpaperInfo;

import java.util.ArrayList;
import java.util.List;

/**
 * Test implementation of {@link CategoryProvider}.
 */
public class TestCategoryProvider implements CategoryProvider {
    private final List<Category> mCategories;

    public TestCategoryProvider() {
        Category category1 = new ImageCategory(
                "My photos" /* title */,
                "image_wallpapers" /* collection */,
                0 /* priority */);

        ArrayList<WallpaperInfo> wallpapers = new ArrayList<>();
        WallpaperInfo wallpaperInfo = new com.android.wallpaper.testing.TestWallpaperInfo(0);
        wallpapers.add(wallpaperInfo);
        Category category2 = new com.android.wallpaper.testing.TestWallpaperCategory(
                "Test category", "init_collection", wallpapers,
                1 /* priority */);

        mCategories = new ArrayList<>();
        mCategories.add(category1);
        mCategories.add(category2);
    }

    @Override
    public void fetchCategories(CategoryReceiver receiver, boolean forceRefresh) {
        // Mimic real behavior by fetching asynchronously.
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                List<Category> categories = getTestCategories();
                for (Category category : categories) {
                    receiver.onCategoryReceived(category);
                }
                receiver.doneFetchingCategories();
            }
        });
    }

    @Override
    public int getSize() {
        return mCategories == null ? 0 : mCategories.size();
    }

    @Override
    public Category getCategory(int index) {
        return mCategories == null ? null : mCategories.get(index);
    }

    @Override
    public Category getCategory(String collectionId) {
        Category category;
        for (int i = 0; i < mCategories.size(); i++) {
            category = mCategories.get(i);
            if (category.getCollectionId().equals(collectionId)) {
                return category;
            }
        }
        return null;
    }

    @Override
    public boolean isCategoriesFetched() {
        return false;
    }

    @Override
    public void resetIfNeeded() {
        mCategories.clear();
    }

    /** Returns a list of test Category objects used by this TestCategoryProvider. */
    public List<Category> getTestCategories() {
        return mCategories;
    }
}
