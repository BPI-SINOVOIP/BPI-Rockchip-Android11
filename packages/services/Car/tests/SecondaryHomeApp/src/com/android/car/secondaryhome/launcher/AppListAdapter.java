/**
 * Copyright (c) 2019 The Android Open Source Project
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

package com.android.car.secondaryhome.launcher;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

import com.android.car.secondaryhome.R;

import java.util.List;
import java.util.Set;

/** Adapter for available apps list. */
public final class AppListAdapter extends ArrayAdapter<AppEntry> {
    private final LayoutInflater mInflater;

    AppListAdapter(@NonNull Context context) {
        super(context, android.R.layout.simple_list_item_2);
        mInflater = LayoutInflater.from(context);
    }

    /**
     * Sets data for AppListAdaptor and exclude the app from  blackList
     * @param data        List of {@link AppEntry}
     * @param blackList   A (possibly empty but not null) list of apps (package names) to hide
     */
    void setData(@NonNull List<AppEntry> data, @NonNull Set<String> blackList) {
        clear();

        data.stream().filter(app -> !blackList.contains(app.getComponentName().getPackageName()))
                .forEach(app -> add(app));

        sort(AppEntry.AppNameComparator);
    }

    @Override
    public View getView(@NonNull int position,
            @Nullable View convertView,
            @NonNull ViewGroup parent) {
        View view;
        if (convertView == null) {
            view = mInflater.inflate(R.layout.app_grid_item, parent, false);
        } else {
            view = convertView;
        }

        AppEntry item = getItem(position);
        if (item != null) {
            ((ImageView) view.findViewById(R.id.app_icon)).setImageDrawable(item.getIcon());
            ((TextView) view.findViewById(R.id.app_name)).setText(item.getLabel());
        }
        return view;
    }
}
