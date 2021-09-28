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

package com.android.car.settings.common;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import com.android.car.settings.R;

/**
 * {@link Preference} for displaying information in the header of a PreferenceScreen.
 * Supports displaying an icon, title, and summary (description).
 */
public class EntityHeaderPreference extends Preference {

    public EntityHeaderPreference(Context context, AttributeSet attrs,
            int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        init();
    }

    public EntityHeaderPreference(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    public EntityHeaderPreference(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public EntityHeaderPreference(Context context) {
        super(context);
        init();
    }

    private void init() {
        setLayoutResource(R.layout.entity_header_preference);
        setSelectable(false);
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);
        View itemView = holder.itemView;
        setIconView(itemView, R.id.entity_header_icon, getIcon());
        setTextView(itemView, R.id.entity_header_title, getTitle());
        setTextView(itemView, R.id.entity_header_summary, getSummary());
    }

    private void setTextView(View view, @IdRes int id, CharSequence text) {
        TextView textView = view.findViewById(id);
        if (textView != null) {
            textView.setText(text);
            textView.setVisibility(TextUtils.isEmpty(text) ? View.GONE : View.VISIBLE);
        }
    }

    private void setIconView(View view, @IdRes int id, Drawable icon) {
        ImageView iconView = view.findViewById(id);
        if (iconView != null) {
            iconView.setImageDrawable(icon);
            iconView.setVisibility(icon == null ? View.GONE : View.VISIBLE);
        }
    }
}
