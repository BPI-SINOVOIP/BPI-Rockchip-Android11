package com.android.settings;

import android.app.Dialog;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.SystemProperties;
import android.provider.Settings;
import android.provider.SearchIndexableResource;
import android.content.res.Resources;

import java.util.ArrayList;
import java.util.List;

import com.android.internal.logging.nano.MetricsProto.MetricsEvent;
import com.android.settings.search.BaseSearchIndexProvider;

import androidx.preference.ListPreference;
import androidx.preference.Preference;
import androidx.preference.SwitchPreference;


public class ScreenshotSettings extends SettingsPreferenceFragment implements Preference.OnPreferenceChangeListener {
    /**
     * Called when the activity is first created.
     */
    private static final String KEY_SCREENSHOT_DELAY = "screenshot_delay";
    private static final String KEY_SCREENSHOT_STORAGE_LOCATION = "screenshot_storage";
    private static final String KEY_SCREENSHOT_SHOW = "screenshot_show";
    private static final String KEY_SCREENSHOT_VERSION = "screenshot_version";

    private ListPreference mDelay;
    private SwitchPreference mShow;

    private SharedPreferences mSharedPreference;
    private SharedPreferences.Editor mEdit;
    private SettingsApplication mScreenshot;

    private Context mContext;
    private Dialog dialog;
    private static final String INTERNAL_STORAGE = "internal_storage";
    private static final String EXTERNAL_SD_STORAGE = "external_sd_storage";
    private static final String EXTERNAL_USB_STORAGE = "internal_usb_storage";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        addPreferencesFromResource(R.xml.screenshot);

        mContext = getActivity();
        mDelay = (ListPreference) findPreference(KEY_SCREENSHOT_DELAY);
        mShow = (SwitchPreference) findPreference(KEY_SCREENSHOT_SHOW);

        mShow.setOnPreferenceChangeListener(this);
        mDelay.setOnPreferenceChangeListener(this);

        mSharedPreference = this.getPreferenceScreen().getSharedPreferences();
        mEdit = mSharedPreference.edit();

        String summary_delay = mDelay.getSharedPreferences().getString("screenshot_delay", "15");
        mDelay.setSummary(summary_delay + getString(R.string.later));
        mDelay.setValue(summary_delay);
        boolean isShow = Settings.System.getInt(mContext.getContentResolver(), "screenshot_button_show", 1) == 1;
        mShow.setChecked(isShow);
        Resources res = mContext.getResources();
        boolean mHasNavigationBar = res.getBoolean(com.android.internal.R.bool.config_showNavigationBar);
        if (!mHasNavigationBar) {
            getPreferenceScreen().removePreference(mShow);
        }

        mScreenshot = (SettingsApplication) getActivity().getApplication();
    }


    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        // TODO Auto-generated method stub
        if (preference == mDelay) {
            int value = Integer.parseInt((String) newValue);
            mDelay.setSummary((String) newValue + getString(R.string.later));
            mScreenshot.startScreenshot(value);
        }
        return true;
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        // TODO Auto-generated method stub
        if (preference == mShow) {
            boolean show = mShow.isChecked();
            Settings.System.putInt(getContentResolver(), "screenshot_button_show", show ? 1 : 0);
        }
        return super.onPreferenceTreeClick(preference);
    }

    @Override
    public int getMetricsCategory() {
        // TODO Auto-generated method stub
        return 5;
    }

    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER = new BaseSearchIndexProvider() {

        @Override
        public List<SearchIndexableResource> getXmlResourcesToIndex(Context context,
            boolean enabled) {
            List<SearchIndexableResource> indexables = new ArrayList<>();
            SearchIndexableResource indexable = new SearchIndexableResource(context);
            indexable.xmlResId = R.xml.screenshot;
            indexables.add(indexable);
            return indexables;
        }
    };
}
