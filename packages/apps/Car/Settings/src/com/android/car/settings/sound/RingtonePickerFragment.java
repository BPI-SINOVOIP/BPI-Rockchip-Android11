/*
 * Copyright (C) 2021 The Android Open Source Project
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

package com.android.car.settings.sound;

import android.content.Context;
import android.media.RingtoneManager;
import android.os.Bundle;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.XmlRes;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.ui.toolbar.MenuItem;

import java.util.Collections;
import java.util.List;

/** Ringtone picker fragment. */
public class RingtonePickerFragment extends SettingsFragment {

    private RingtonePickerPreferenceController mPreferenceController;
    private MenuItem mSaveButton;

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.ringtone_picker_fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);

        mPreferenceController = use(RingtonePickerPreferenceController.class,
                R.string.pk_ringtone_picker);
        mPreferenceController.setArguments(getArguments());
        mSaveButton = new MenuItem.Builder(getContext())
                .setTitle(R.string.ringtone_picker_save_title)
                .setOnClickListener(item -> {
                    mPreferenceController.saveRingtone();
                    goBack();
                })
                .build();
    }

    @Override
    protected List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mSaveButton);
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        getToolbar().setTitle(getArguments().getCharSequence(RingtoneManager.EXTRA_RINGTONE_TITLE));
    }

    @Override
    public void onResume() {
        super.onResume();

        // Logic to scroll to the selected item. This needs to be done in a global layout listener
        // so that it can be triggered after the sound items added dynamically in the
        // PreferenceScreen.
        getListView().getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        // This should only be triggered once per resume.
                        getListView().getViewTreeObserver().removeOnGlobalLayoutListener(this);

                        // There are various methods on the PreferenceFragment and RecyclerView
                        // that should be able to scroll to the desired preference. However this
                        // approach is the most reliable with dynamically added preferences.
                        LinearLayoutManager layoutManager =
                                (LinearLayoutManager) getListView().getLayoutManager();
                        layoutManager.scrollToPositionWithOffset(
                                mPreferenceController.getCurrentlySelectedPreferencePos(),
                                /* offset= */ 0);

                        // This will only work after the scrolling has completed, since the item
                        // may not be immediately visible. Setting this item to be selected to allow
                        // this item to be rotary focused by default if in rotary mode.
                        getListView().post(() -> {
                            View itemView = getListView().findViewById(
                                    R.id.ringtone_picker_selected_id);
                            itemView = layoutManager.findContainingItemView(itemView);
                            itemView.setSelected(true);
                        });
                    }
                });
    }
}
