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

package com.android.documentsui.sidebar;

import static com.android.documentsui.base.SharedMinimal.DEBUG;

import android.util.Log;
import android.view.View;
import android.widget.TextView;

import com.android.documentsui.R;
import com.android.documentsui.base.UserId;

/**
 * An {@link Item} for displaying text in the side bar.
 */
class HeaderItem extends Item {
    private static final String TAG = "HeaderItem";

    private static final String STRING_ID = "HeaderItem";

    HeaderItem(String title) {
        super(R.layout.item_root_header, title, STRING_ID, UserId.UNSPECIFIED_USER);
    }

    @Override
    void bindView(View convertView) {
        final TextView titleView = convertView.findViewById(android.R.id.title);
        titleView.setText(title);
    }

    @Override
    boolean isRoot() {
        return false;
    }

    @Override
    void open() {
        if (DEBUG) {
            Log.d(TAG, "Ignoring click/hover on spacer item.");
        }
    }
}
