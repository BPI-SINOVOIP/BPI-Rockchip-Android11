/*
 * Copyright (C) 2016 The Android Open Source Project
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

import android.view.DragEvent;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;

import com.android.documentsui.MenuManager;
import com.android.documentsui.R;
import com.android.documentsui.base.UserId;

/**
 * Describes a root navigation point of documents. Each one of them is presented as an item in the
 * sidebar
 */
public abstract class Item {
    private final @LayoutRes int mLayoutId;

    public final String title;
    public final UserId userId;
    final String stringId;

    public Item(@LayoutRes int layoutId, String title, String stringId, UserId userId) {
        mLayoutId = layoutId;
        this.title = title;
        this.stringId = stringId;
        this.userId = userId;
    }

    public View getView(View convertView, ViewGroup parent) {
        if (convertView == null
                || (Integer) convertView.getTag(R.id.layout_id_tag) != mLayoutId) {
            convertView = LayoutInflater.from(parent.getContext())
                    .inflate(mLayoutId, parent, false);
        }
        convertView.setTag(R.id.layout_id_tag, mLayoutId);
        bindView(convertView);
        return convertView;
    }

    abstract void bindView(View convertView);

    abstract boolean isRoot();

    abstract void open();

    /**
     * Get the package name string.
     */
    public String getPackageName() {
        return "";
    }

    /**
     * Get the root or app summary such as account information.
     */
    public @Nullable String getSummary() {
        return null;
    }

    boolean isDropTarget() {
        return isRoot();
    }

    boolean dropOn(DragEvent event) {
        return false;
    }

    boolean showAppDetails() {
        return false;
    }

    void createContextMenu(Menu menu, MenuInflater inflater, MenuManager menuManager) {}
}
