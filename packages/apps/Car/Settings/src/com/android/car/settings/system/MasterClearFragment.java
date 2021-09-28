/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.car.settings.system;

import static android.app.Activity.RESULT_OK;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.annotation.XmlRes;
import androidx.recyclerview.widget.RecyclerView;

import com.android.car.settings.R;
import com.android.car.settings.common.ActivityResultCallback;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.settings.security.CheckLockActivity;
import com.android.car.ui.toolbar.MenuItem;

import java.util.Collections;
import java.util.List;

/**
 * Presents the user with the option to reset the head unit to its default "factory" state. If a
 * user confirms, the user is first required to authenticate and then presented with a secondary
 * confirmation: {@link MasterClearConfirmFragment}. The user must scroll to the bottom of the page
 * before proceeding.
 */
public class MasterClearFragment extends SettingsFragment implements ActivityResultCallback {

    // Arbitrary request code for starting CheckLockActivity when the reset button is clicked.
    @VisibleForTesting
    static final int CHECK_LOCK_REQUEST_CODE = 88;

    private MenuItem mMasterClearButton;

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mMasterClearButton);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mMasterClearButton = new MenuItem.Builder(getContext())
                .setTitle(R.string.master_clear_button_text)
                .setEnabled(false)
                .setOnClickListener(i ->
                        startActivityForResult(new Intent(getContext(), CheckLockActivity.class),
                                CHECK_LOCK_REQUEST_CODE, /* callback= */ MasterClearFragment.this))
                .build();
    }

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.master_clear_fragment;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        RecyclerView recyclerView = getListView();
        recyclerView.getViewTreeObserver().addOnGlobalLayoutListener(
                new ViewTreeObserver.OnGlobalLayoutListener() {
                    @Override
                    public void onGlobalLayout() {
                        mMasterClearButton.setEnabled(isAtEnd());
                        recyclerView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                    }
                });
        recyclerView.setOnScrollChangeListener((v, scrollX, scrollY, oldScrollX, oldScrollY) -> {
            if (isAtEnd()) {
                mMasterClearButton.setEnabled(true);
            }
        });
    }

    @Override
    public void processActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (requestCode == CHECK_LOCK_REQUEST_CODE && resultCode == RESULT_OK) {
            launchFragment(new MasterClearConfirmFragment());
        }
    }

    /** Returns {@code true} if the RecyclerView is completely displaying the last item. */
    private boolean isAtEnd() {
        RecyclerView recyclerView = getListView();
        RecyclerView.LayoutManager layoutManager = recyclerView.getLayoutManager();
        if (layoutManager == null || layoutManager.getChildCount() == 0) {
            return true;
        }

        int childCount = layoutManager.getChildCount();
        View lastVisibleChild = layoutManager.getChildAt(childCount - 1);

        // The list has reached the bottom if the last child that is visible is the last item
        // in the list and it's fully shown.
        return layoutManager.getPosition(lastVisibleChild) == (layoutManager.getItemCount() - 1)
                && layoutManager.getDecoratedBottom(lastVisibleChild) <= layoutManager.getHeight();
    }
}
