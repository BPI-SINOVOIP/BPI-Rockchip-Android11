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

import android.annotation.Nullable;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.Settings;
import android.text.TextUtils;

import androidx.fragment.app.Fragment;

import com.android.car.settings.accounts.AccountSettingsFragment;
import com.android.car.settings.accounts.ChooseAccountFragment;
import com.android.car.settings.applications.ApplicationDetailsFragment;
import com.android.car.settings.applications.ApplicationsSettingsFragment;
import com.android.car.settings.applications.AppsAndNotificationsFragment;
import com.android.car.settings.applications.defaultapps.DefaultAutofillPickerFragment;
import com.android.car.settings.applications.specialaccess.ModifySystemSettingsFragment;
import com.android.car.settings.applications.specialaccess.NotificationAccessFragment;
import com.android.car.settings.applications.specialaccess.PremiumSmsAccessFragment;
import com.android.car.settings.applications.specialaccess.SpecialAccessSettingsFragment;
import com.android.car.settings.applications.specialaccess.UsageAccessFragment;
import com.android.car.settings.applications.specialaccess.WifiControlFragment;
import com.android.car.settings.bluetooth.BluetoothSettingsFragment;
import com.android.car.settings.datausage.DataUsageFragment;
import com.android.car.settings.datetime.DatetimeSettingsFragment;
import com.android.car.settings.display.DisplaySettingsFragment;
import com.android.car.settings.home.HomepageFragment;
import com.android.car.settings.inputmethod.KeyboardFragment;
import com.android.car.settings.language.LanguagePickerFragment;
import com.android.car.settings.language.LanguagesAndInputFragment;
import com.android.car.settings.location.LocationSettingsFragment;
import com.android.car.settings.network.MobileNetworkFragment;
import com.android.car.settings.network.MobileNetworkListFragment;
import com.android.car.settings.network.NetworkAndInternetFragment;
import com.android.car.settings.privacy.PrivacySettingsFragment;
import com.android.car.settings.quicksettings.QuickSettingFragment;
import com.android.car.settings.security.SecuritySettingsFragment;
import com.android.car.settings.sound.RingtonePickerFragment;
import com.android.car.settings.sound.SoundSettingsFragment;
import com.android.car.settings.storage.StorageSettingsFragment;
import com.android.car.settings.system.AboutSettingsFragment;
import com.android.car.settings.system.LegalInformationFragment;
import com.android.car.settings.system.ResetOptionsFragment;
import com.android.car.settings.system.SystemSettingsFragment;
import com.android.car.settings.tts.TextToSpeechOutputFragment;
import com.android.car.settings.units.UnitsSettingsFragment;
import com.android.car.settings.users.UserDetailsFragment;
import com.android.car.settings.users.UsersListFragment;
import com.android.car.settings.wifi.AddWifiFragment;
import com.android.car.settings.wifi.WifiSettingsFragment;
import com.android.car.settings.wifi.WifiTetherFragment;
import com.android.car.settings.wifi.preferences.WifiPreferencesFragment;

/**
 * Top level settings class, containing static instances of CarSettings activities.
 */
public class CarSettingActivities {

    private static final Logger LOG = new Logger(CarSettingActivities.class);

    /**
     * QuickSettings Activity.
     */
    public static class QuickSettingActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new QuickSettingFragment();
        }
    }

    /**
     * Homepage Activity.
     */
    public static class HomepageActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new HomepageFragment();
        }
    }

    // Homepage sub-sections

    /**
     * Display Settings Activity.
     */
    public static class DisplaySettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new DisplaySettingsFragment();
        }
    }

    /**
     * Sound Settings Activity.
     */
    public static class SoundSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new SoundSettingsFragment();
        }
    }

    /**
     * Ringtone Picker Activity.
     */
    public static class RingtonePickerActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            Bundle args = getIntent().getExtras().deepCopy();
            Fragment fragment = new RingtonePickerFragment();
            fragment.setArguments(args);
            return fragment;
        }
    }

    /**
     * Network and Internet Activity.
     */
    public static class NetworkAndInternetActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new NetworkAndInternetFragment();
        }
    }

    /**
     * Bluetooth Settings Activity.
     */
    public static class BluetoothSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new BluetoothSettingsFragment();
        }
    }

    /**
     * Units Settings Activity.
     */
    public static class UnitsSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new UnitsSettingsFragment();
        }
    }

    /**
     * Location Settings Activity.
     */
    public static class LocationSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new LocationSettingsFragment();
        }
    }

    /**
     * Apps and Notifications Activity.
     */
    public static class AppsAndNotificationsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new AppsAndNotificationsFragment();
        }
    }

    /**
     * Datetime Settings Activity.
     */
    public static class DatetimeSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new DatetimeSettingsFragment();
        }
    }

    /**
     * User Details Activity.
     * The initial fragment shown is based on whether the current user is an admin.
     */
    public static class UserDetailsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            UserManager userManager = UserManager.get(this);
            if (userManager.isAdminUser()) {
                // Admins can see a full list of users in Settings.
                LOG.v("Creating UsersListFragment for admin user.");
                return new UsersListFragment();
            } else {
                // Non-admins can only manage themselves in Settings.
                LOG.v("Creating UserDetailsFragment for non-admin.");
                return UserDetailsFragment.newInstance(UserHandle.myUserId());
            }
        }
    }

    /**
     * Account Settings Activity.
     */
    public static class AccountSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new AccountSettingsFragment();
        }
    }

    /**
     * Privacy Settings Activity.
     */
    public static class PrivacySettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new PrivacySettingsFragment();
        }
    }

    /**
     * Storage Settings Activity.
     */
    public static class StorageSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new StorageSettingsFragment();
        }
    }

    /**
     * Security Settings Activity.
     */
    public static class SecuritySettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new SecuritySettingsFragment();
        }
    }

    /**
     * System Settings Activity.
     */
    public static class SystemSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new SystemSettingsFragment();
        }
    }

    // Network & internet sub-sections

    /**
     * Wifi Settings Activity.
     */
    public static class WifiSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new WifiSettingsFragment();
        }
    }

    /**
     * Wifi Hotspot and Tethering Activity.
     */
    public static class WifiTetherActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new WifiTetherFragment();
        }
    }

    /**
     * Mobile Network Activity.
     */
    public static class MobileNetworkActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return MobileNetworkFragment.newInstance(
                    getIntent().getIntExtra("network_sub_id", Integer.MIN_VALUE));
        }
    }

    /**
     * Mobile Network List Activity.
     */
    public static class MobileNetworkListActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new MobileNetworkListFragment();
        }
    }

    /**
     * Mobile Data Usage Activity.
     */
    public static class DataUsageActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new DataUsageFragment();
        }
    }

    // Apps & Notifications sub-sections

    /**
     * Application Settings Activity.
     */
    public static class ApplicationsSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new ApplicationsSettingsFragment();
        }
    }

    /**
     * Application Special Access Settings Activity.
     */
    public static class SpecialAccessSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new SpecialAccessSettingsFragment();
        }
    }

    // System sub-sections

    /**
     * Languages and Input Activity.
     */
    public static class LanguagesAndInputActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new LanguagesAndInputFragment();
        }
    }

    /**
     * About Settings Activity.
     */
    public static class AboutSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new AboutSettingsFragment();
        }
    }

    /**
     * Legal Information Activity.
     */
    public static class LegalInformationActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new LegalInformationFragment();
        }
    }

    /**
     * Reset Options Activity.
     */
    public static class ResetOptionsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new ResetOptionsFragment();
        }
    }

    // Wi-fi sub-sections

    /**
     * Add Wifi Network Activity.
     */
    public static class AddWifiActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new AddWifiFragment();
        }
    }

    /**
     * Wifi Preferences Activity.
     */
    public static class WifiPreferencesActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new WifiPreferencesFragment();
        }
    }

    // App Info sub-sections

    /**
     * Application Details Activity.
     * Intent required to include package string as Settings.EXTRA_APP_PACKAGE extra.
     */
    public static class ApplicationsDetailsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            Intent intent = getIntent();
            String pkg = intent.getStringExtra(Settings.EXTRA_APP_PACKAGE);
            if (TextUtils.isEmpty(pkg)) {
                LOG.w("No package provided for application detailed intent");
                Uri uri = intent.getData();
                if (uri == null) {
                    LOG.w("No uri provided for application detailed intent");
                    return null;
                }
                pkg = uri.getSchemeSpecificPart();
            }
            return ApplicationDetailsFragment.getInstance(pkg);
        }
    }

    // Special app access sub-section

    /**
     * App Modify System Settings Access Activity.
     */
    public static class ModifySystemSettingsActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new ModifySystemSettingsFragment();
        }
    }

    /**
     * App Notification Access Activity.
     */
    public static class NotificationAccessActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new NotificationAccessFragment();
        }
    }

    /**
     * App Premium SMS Access Activity.
     */
    public static class PremiumSmsAccessActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new PremiumSmsAccessFragment();
        }
    }

    /**
     * App Usage Access Activity.
     */
    public static class UsageAccessActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new UsageAccessFragment();
        }
    }

    /**
     * App Wifi Control Access Activity.
     */
    public static class WifiControlActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new WifiControlFragment();
        }
    }

    // Account sub-section

    /**
     * Choose Account Activity.
     */
    public static class ChooseAccountActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new ChooseAccountFragment();
        }
    }

    // Languages & input sub-section

    /**
     * Language Picker Activity.
     */
    public static class LanguagePickerActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new LanguagePickerFragment();
        }
    }

    /**
     * Default Autofill Picker Activity.
     */
    public static class DefaultAutofillPickerActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new DefaultAutofillPickerFragment();
        }
    }

    /**
     * Keyboard Activity.
     */
    public static class KeyboardActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new KeyboardFragment();
        }
    }

    /**
     * Text to Speech Output Activity.
     */
    public static class TextToSpeechOutputActivity extends BaseCarSettingsActivity {
        @Nullable
        @Override
        protected Fragment getInitialFragment() {
            return new TextToSpeechOutputFragment();
        }
    }

}
