/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.car.dialer.ui.common;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.ViewModelProviders;

import com.android.car.dialer.R;
import com.android.car.dialer.ui.TelecomActivityViewModel;
import com.android.car.ui.baselayout.Insets;
import com.android.car.ui.baselayout.InsetsChangedListener;
import com.android.car.ui.core.CarUi;
import com.android.car.ui.toolbar.Toolbar;
import com.android.car.ui.toolbar.ToolbarController;

/**
 * The base class for top level dialer content {@link Fragment}s.
 */
public abstract class DialerBaseFragment extends Fragment implements InsetsChangedListener {

    /**
     * Interface for Dialer top level fragment's parent to implement.
     */
    public interface DialerFragmentParent {

        /**
         * Push a fragment to the back stack. Update action bar accordingly.
         */
        void pushContentFragment(Fragment fragment, String fragmentTag);
    }

    @CallSuper
    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        ToolbarController toolbar = CarUi.getToolbar(requireActivity());
        // Null check for unit tests to pass
        if (toolbar != null) {
            setupToolbar(toolbar);
        }

        Insets insets = CarUi.getInsets(requireActivity());
        // Null check for unit tests to pass
        if (insets != null) {
            onCarUiInsetsChanged(insets);
        }
    }

    /**
     * Customizes the tool bar. Can be overridden in subclasses.
     */
    protected void setupToolbar(@NonNull ToolbarController toolbar) {
        TelecomActivityViewModel viewModel = ViewModelProviders.of(requireActivity()).get(
                TelecomActivityViewModel.class);
        LiveData<String> toolbarTitleLiveData = viewModel.getToolbarTitle();
        toolbarTitleLiveData.observe(this, toolbar::setTitle);

        toolbar.setState(getToolbarState());
        toolbar.setLogo(getToolbarState() == Toolbar.State.HOME
                ? requireActivity().getDrawable(R.drawable.ic_app_icon)
                : null);

        toolbar.setMenuItems(R.xml.menuitems);
    }

    /**
     * Push a fragment to the back stack. Update action bar accordingly.
     */
    protected void pushContentFragment(@NonNull Fragment fragment, String fragmentTag) {
        Activity parentActivity = getActivity();
        if (parentActivity instanceof DialerFragmentParent) {
            ((DialerFragmentParent) parentActivity).pushContentFragment(fragment, fragmentTag);
        }
    }

    protected Toolbar.State getToolbarState() {
        return Toolbar.State.HOME;
    }

    @Override
    public void onCarUiInsetsChanged(Insets insets) {
        requireView().setPadding(insets.getLeft(), insets.getTop(),
                insets.getRight(), insets.getBottom());
    }
}
