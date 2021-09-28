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
package com.android.wallpaper.model;

import android.app.Activity;
import android.content.Context;

import com.android.wallpaper.asset.Asset;

/**
 * A placeholder {@link Category} with only id and title (and no content).
 * Typically used to display partially loaded categories.
 */
public class PlaceholderCategory extends Category {

    /**
     * Constructs an EmptyCategory object.
     *
     * @see Category#Category(String, String, int)
     */
    public PlaceholderCategory(String title, String collectionId, int priority) {
        super(title, collectionId, priority);
    }

    @Override
    public void show(Activity srcActivity, PickerIntentFactory factory, int requestCode) {

    }

    @Override
    public Asset getThumbnail(Context context) {
        return null;
    }
}
