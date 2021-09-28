package com.android.tv.settings.connectivity;

import android.app.Fragment;

import com.android.tv.settings.BaseSettingsFragment;
import com.android.tv.settings.TvSettingsActivity;

public class HotPotActivity extends TvSettingsActivity {

    @Override
    protected Fragment createSettingsFragment() {
        return SettingsFragment.newInstance();
    }

    public static class SettingsFragment extends BaseSettingsFragment {

        public static SettingsFragment newInstance() {
            return new SettingsFragment();
        }

        @Override
        public void onPreferenceStartInitialScreen() {
            final HotPotFragment fragment = HotPotFragment.newInstance();
            startPreferenceFragment(fragment);
        }
    }
}