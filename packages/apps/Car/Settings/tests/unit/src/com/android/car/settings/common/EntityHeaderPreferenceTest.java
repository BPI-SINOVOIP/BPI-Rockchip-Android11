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

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.preference.PreferenceViewHolder;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.settings.R;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class EntityHeaderPreferenceTest {

    private Context mContext = ApplicationProvider.getApplicationContext();
    private View mRootView;
    private EntityHeaderPreference mPref;
    private PreferenceViewHolder mHolder;

    @Before
    public void setUp() {
        mRootView = View.inflate(mContext, R.layout.entity_header_preference, /* parent= */ null);
        mHolder = PreferenceViewHolder.createInstanceForTests(mRootView);
        mPref = new EntityHeaderPreference(mContext);
    }

    @Test
    public void onBindViewHolder_noSetIcon_shouldNotBeVisible() {
        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.entity_header_icon).getVisibility())
                .isEqualTo(View.GONE);
    }

    @Test
    public void onBindViewHolder_noSetTitle_shouldNotBeVisible() {
        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.entity_header_title).getVisibility())
                .isEqualTo(View.GONE);
    }

    @Test
    public void onBindViewHolder_noSetSummary_shouldNotBeVisible() {
        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.entity_header_summary).getVisibility())
                .isEqualTo(View.GONE);
    }

    @Test
    public void onBindViewHolder_setIcon_shouldShowIcon() {
        mPref.setIcon(R.drawable.ic_lock);

        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.entity_header_icon).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(((ImageView) mRootView.findViewById(R.id.entity_header_icon)).getDrawable())
                .isNotNull();
    }

    @Test
    public void onBindViewHolder_setLabel_shouldShowSameText() {
        mPref.setTitle(mContext.getText(R.string.settings_label));

        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.entity_header_title).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(((TextView) mRootView.findViewById(R.id.entity_header_title)).getText())
                .isEqualTo(mContext.getText(R.string.settings_label));
    }

    @Test
    public void onBindViewHolder_setSummary_shouldShowSameText() {
        mPref.setSummary(mContext.getText(R.string.settings_label));

        mPref.onBindViewHolder(mHolder);

        assertThat(mRootView.findViewById(R.id.entity_header_summary).getVisibility())
                .isEqualTo(View.VISIBLE);
        assertThat(((TextView) mRootView.findViewById(R.id.entity_header_summary)).getText())
                .isEqualTo(mContext.getText(R.string.settings_label));
    }
}
