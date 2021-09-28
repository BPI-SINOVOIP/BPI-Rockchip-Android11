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

package com.android.tv.twopanelsettings.slices;

import static androidx.lifecycle.Lifecycle.Event.ON_CREATE;
import static androidx.lifecycle.Lifecycle.Event.ON_DESTROY;
import static androidx.lifecycle.Lifecycle.Event.ON_PAUSE;
import static androidx.lifecycle.Lifecycle.Event.ON_RESUME;
import static androidx.lifecycle.Lifecycle.Event.ON_START;
import static androidx.lifecycle.Lifecycle.Event.ON_STOP;

import static com.android.tv.twopanelsettings.slices.InstrumentationUtils.logPageFocused;

import android.app.tvsettings.TvSettingsEnums;
import android.content.Context;
import android.os.Bundle;
import android.view.Gravity;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.lifecycle.LifecycleOwner;
import androidx.preference.PreferenceScreen;

import com.android.settingslib.core.instrumentation.Instrumentable;
import com.android.settingslib.core.instrumentation.MetricsFeatureProvider;
import com.android.settingslib.core.instrumentation.VisibilityLoggerMixin;
import com.android.settingslib.core.lifecycle.Lifecycle;
import com.android.tv.twopanelsettings.R;
import com.android.tv.twopanelsettings.SettingsPreferenceFragmentBase;
import com.android.tv.twopanelsettings.TwoPanelSettingsFragment;

/**
 * A copy of SettingsPreferenceFragment in Settings.
 */
public abstract class SettingsPreferenceFragment extends SettingsPreferenceFragmentBase
        implements LifecycleOwner, Instrumentable,
        TwoPanelSettingsFragment.PreviewableComponentCallback {
    private final Lifecycle mLifecycle = new Lifecycle(this);
    private final VisibilityLoggerMixin mVisibilityLoggerMixin;
    protected MetricsFeatureProvider mMetricsFeatureProvider;

    @NonNull
    public Lifecycle getLifecycle() {
        return mLifecycle;
    }

    public SettingsPreferenceFragment() {
        mMetricsFeatureProvider = new MetricsFeatureProvider();
        // Mixin that logs visibility change for activity.
        mVisibilityLoggerMixin = new VisibilityLoggerMixin(getMetricsCategory(),
                mMetricsFeatureProvider);
        getLifecycle().addObserver(mVisibilityLoggerMixin);
    }

    @CallSuper
    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mLifecycle.onAttach(context);
    }

    @CallSuper
    @Override
    public void onCreate(Bundle savedInstanceState) {
        mLifecycle.onCreate(savedInstanceState);
        mLifecycle.handleLifecycleEvent(ON_CREATE);
        super.onCreate(savedInstanceState);
        if (getCallbackFragment() != null
                && !(getCallbackFragment() instanceof TwoPanelSettingsFragment)) {
            logPageFocused(getPageId(), true);
        }
    }

    // We explicitly set the title gravity to RIGHT in RTL cases to remedy some complicated gravity
    // issues. For more details, please read the comment of onViewCreated() in
    // com.android.tv.settings.SettingsPreferenceFragment.
    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        if (view != null) {
            TextView titleView = view.findViewById(R.id.decor_title);
            // We rely on getResources().getConfiguration().getLayoutDirection() instead of
            // view.isLayoutRtl() as the latter could return false in some complex scenarios even if
            // it is RTL.
            if (titleView != null
                    && getResources().getConfiguration().getLayoutDirection()
                            == View.LAYOUT_DIRECTION_RTL) {
                titleView.setGravity(Gravity.RIGHT);
            }
        }
    }

    @Override
    public void setPreferenceScreen(PreferenceScreen preferenceScreen) {
        mLifecycle.setPreferenceScreen(preferenceScreen);
        super.setPreferenceScreen(preferenceScreen);
    }

    @CallSuper
    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mLifecycle.onSaveInstanceState(outState);
    }

    @CallSuper
    @Override
    public void onStart() {
        mLifecycle.handleLifecycleEvent(ON_START);
        super.onStart();
    }

    @CallSuper
    @Override
    public void onResume() {
        mVisibilityLoggerMixin.setSourceMetricsCategory(getActivity());
        super.onResume();
        mLifecycle.handleLifecycleEvent(ON_RESUME);
    }

    // This should only be invoked if the parent Fragment is TwoPanelSettingsFragment.
    @CallSuper
    @Override
    public void onArriveAtMainPanel(boolean forward) {
        logPageFocused(getPageId(), forward);
    }

    @CallSuper
    @Override
    public void onPause() {
        mLifecycle.handleLifecycleEvent(ON_PAUSE);
        super.onPause();
    }

    @CallSuper
    @Override
    public void onStop() {
        mLifecycle.handleLifecycleEvent(ON_STOP);
        super.onStop();
    }

    @CallSuper
    @Override
    public void onDestroy() {
        mLifecycle.handleLifecycleEvent(ON_DESTROY);
        super.onDestroy();
    }

    @CallSuper
    @Override
    public void onCreateOptionsMenu(final Menu menu, final MenuInflater inflater) {
        mLifecycle.onCreateOptionsMenu(menu, inflater);
        super.onCreateOptionsMenu(menu, inflater);
    }

    @CallSuper
    @Override
    public void onPrepareOptionsMenu(final Menu menu) {
        mLifecycle.onPrepareOptionsMenu(menu);
        super.onPrepareOptionsMenu(menu);
    }

    @CallSuper
    @Override
    public boolean onOptionsItemSelected(final MenuItem menuItem) {
        boolean lifecycleHandled = mLifecycle.onOptionsItemSelected(menuItem);
        if (!lifecycleHandled) {
            return super.onOptionsItemSelected(menuItem);
        }
        return lifecycleHandled;
    }

    /** Subclasses should override this to use their own PageId for statsd logging. */
    protected int getPageId() {
        return TvSettingsEnums.PAGE_CLASSIC_DEFAULT;
    }
}
