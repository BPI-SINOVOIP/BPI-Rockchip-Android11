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

package com.google.android.car.kitchensink.rotary;

import android.app.ActivityManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.Switch;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.google.android.car.kitchensink.R;

import java.util.HashSet;
import java.util.Set;

/** Test fragment to enable/disable various components related to RotaryController. */
public final class RotaryFragment extends Fragment {

    private static final String TAG = RotaryFragment.class.getSimpleName();
    private static final String ROTARY_CONTROLLER_PACKAGE = "com.android.car.rotary";
    private static final ComponentName ROTARY_SERVICE_NAME = ComponentName.unflattenFromString(
            ROTARY_CONTROLLER_PACKAGE + "/com.android.car.rotary.RotaryService");
    private static final String ROTARY_PLAYGROUND_PACKAGE = "com.android.car.rotaryplayground";
    private static final String ROTARY_IME_PACKAGE = "com.android.car.rotaryime";

    private static final String ACCESSIBILITY_DELIMITER = ":";

    private static final int ON = 1;
    private static final int OFF = 0;

    private final IntentFilter mFilter = new IntentFilter();

    private TextView mUnavailableMessage;
    private Button mEnableAllButton;
    private Button mDisableAllButton;
    private Switch mRotaryServiceToggle;
    private Switch mRotaryImeToggle;
    private Switch mRotaryPlaygroundToggle;

    private final BroadcastReceiver mPackagesUpdatedReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            switch (action) {
                case Intent.ACTION_PACKAGE_ADDED:
                case Intent.ACTION_PACKAGE_CHANGED:
                case Intent.ACTION_PACKAGE_REMOVED:
                    refreshUi();
                    break;
            }
        }
    };

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mFilter.addAction(Intent.ACTION_PACKAGE_ADDED);
        mFilter.addAction(Intent.ACTION_PACKAGE_CHANGED);
        mFilter.addAction(Intent.ACTION_PACKAGE_REMOVED);
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.rotary_fragment, container, /* attachToRoot= */ false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        mUnavailableMessage = view.findViewById(R.id.unavailable_message);

        mEnableAllButton = view.findViewById(R.id.enable_all_rotary);
        mDisableAllButton = view.findViewById(R.id.disable_all_rotary);

        mRotaryServiceToggle = view.findViewById(R.id.rotary_service_toggle);
        mRotaryImeToggle = view.findViewById(R.id.rotary_ime_toggle);
        mRotaryPlaygroundToggle = view.findViewById(R.id.rotary_playground_toggle);

        mRotaryServiceToggle.setOnCheckedChangeListener(
                (buttonView, isChecked) -> enableRotaryService(getContext(), isChecked));
        mRotaryImeToggle.setOnCheckedChangeListener(
                (buttonView, isChecked) -> enableApplication(getContext(), ROTARY_IME_PACKAGE,
                        isChecked));
        mRotaryPlaygroundToggle.setOnCheckedChangeListener(
                (buttonView, isChecked) -> enableApplication(getContext(),
                        ROTARY_PLAYGROUND_PACKAGE, isChecked));

        mEnableAllButton.setOnClickListener(v -> {
            if (mRotaryServiceToggle.isEnabled()) {
                mRotaryServiceToggle.setChecked(true);
            }
            if (mRotaryImeToggle.isEnabled()) {
                mRotaryImeToggle.setChecked(true);
            }
            if (mRotaryPlaygroundToggle.isEnabled()) {
                mRotaryPlaygroundToggle.setChecked(true);
            }
        });

        mDisableAllButton.setOnClickListener(v -> {
            if (mRotaryServiceToggle.isEnabled()) {
                mRotaryServiceToggle.setChecked(false);
            }
            if (mRotaryImeToggle.isEnabled()) {
                mRotaryImeToggle.setChecked(false);
            }
            if (mRotaryPlaygroundToggle.isEnabled()) {
                mRotaryPlaygroundToggle.setChecked(false);
            }
        });
    }

    @Override
    public void onStart() {
        super.onStart();

        getContext().registerReceiver(mPackagesUpdatedReceiver, mFilter);
    }

    @Override
    public void onResume() {
        super.onResume();

        refreshUi();
    }

    private void refreshUi() {
        ApplicationInfo info = findApplication(getContext(), ROTARY_CONTROLLER_PACKAGE);
        boolean rotaryApplicationExists = info != null;
        info = findApplication(getContext(), ROTARY_IME_PACKAGE);
        boolean rotaryImeExists = info != null;
        info = findApplication(getContext(), ROTARY_PLAYGROUND_PACKAGE);
        boolean rotaryPlaygroundExists = info != null;

        mUnavailableMessage.setVisibility(!rotaryApplicationExists ? View.VISIBLE : View.GONE);

        mEnableAllButton.setEnabled(rotaryApplicationExists);
        mDisableAllButton.setEnabled(rotaryApplicationExists);
        mRotaryServiceToggle.setEnabled(rotaryApplicationExists);
        mRotaryImeToggle.setEnabled(rotaryApplicationExists && rotaryImeExists);
        mRotaryPlaygroundToggle.setEnabled(rotaryApplicationExists && rotaryPlaygroundExists);

        mRotaryServiceToggle.setChecked(isRotaryServiceEnabled(getContext()));
        mRotaryImeToggle.setChecked(
                rotaryImeExists && isApplicationEnabled(getContext(), ROTARY_IME_PACKAGE));
        mRotaryPlaygroundToggle.setChecked(
                rotaryPlaygroundExists && isApplicationEnabled(getContext(),
                        ROTARY_PLAYGROUND_PACKAGE));
    }

    @Override
    public void onStop() {
        super.onStop();

        getContext().unregisterReceiver(mPackagesUpdatedReceiver);
    }

    @Nullable
    private static ApplicationInfo findApplication(Context context, String packageName) {
        PackageManager pm = context.getPackageManager();
        try {
            Log.d(TAG, "Searching for: " + packageName);
            return pm.getApplicationInfoAsUser(packageName, /* flags= */ 0,
                    ActivityManager.getCurrentUser());
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Could not find: " + packageName);
            return null;
        }
    }

    private static boolean isApplicationEnabled(Context context, String packageName) {
        ApplicationInfo info = findApplication(context, packageName);
        if (info == null) {
            return false;
        }

        return info.enabled;
    }

    private static void enableApplication(Context context, String packageName, boolean enable) {
        // Check that the application exists.
        ApplicationInfo info = findApplication(context, packageName);
        if (info == null) {
            Log.e(TAG, "Cannot enable application. " + packageName + " package does not exist");
            return;
        }

        PackageManager pm = context.getPackageManager();
        int currentState = pm.getApplicationEnabledSetting(packageName);
        int desiredState = enable ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;

        boolean isEnabled = currentState == PackageManager.COMPONENT_ENABLED_STATE_ENABLED;
        if (isEnabled != enable
                || currentState == PackageManager.COMPONENT_ENABLED_STATE_DEFAULT) {
            Log.d(TAG, "Application state updated for " + packageName + ": " + enable);
            pm.setApplicationEnabledSetting(packageName, desiredState, /* flags= */ 0);
        }
    }

    private static boolean isRotaryServiceEnabled(Context context) {
        ApplicationInfo info = findApplication(context, ROTARY_CONTROLLER_PACKAGE);
        if (info == null) {
            return false;
        }

        int isAccessibilityEnabled = Settings.Secure.getInt(context.getContentResolver(),
                Settings.Secure.ACCESSIBILITY_ENABLED, OFF);
        if (isAccessibilityEnabled != ON) {
            return false;
        }

        String services = Settings.Secure.getString(context.getContentResolver(),
                Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (services == null) {
            return false;
        }

        return services.contains(ROTARY_CONTROLLER_PACKAGE);
    }

    private static void enableRotaryService(Context context, boolean enable) {
        // Check that the RotaryController exists.
        ApplicationInfo info = findApplication(context, ROTARY_CONTROLLER_PACKAGE);
        if (info == null) {
            Log.e(TAG, "Cannot enable rotary service. " + ROTARY_CONTROLLER_PACKAGE
                    + " package does not exist");
            return;
        }

        // Set the list of accessibility services.
        String services = Settings.Secure.getString(context.getContentResolver(),
                Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
        if (services == null) {
            services = "";
        }
        Log.d(TAG, "Current list of accessibility services: " + services);

        String[] servicesArray = services.split(ACCESSIBILITY_DELIMITER);
        Set<ComponentName> servicesSet = new HashSet<>();
        for (String service : servicesArray) {
            ComponentName name = ComponentName.unflattenFromString(service);
            if (name != null) {
                servicesSet.add(name);
            }
        }

        if (enable) {
            servicesSet.add(ROTARY_SERVICE_NAME);
        } else {
            servicesSet.remove(ROTARY_SERVICE_NAME);
        }

        StringBuilder builder = new StringBuilder();
        for (ComponentName service : servicesSet) {
            if (builder.length() > 0) {
                builder.append(ACCESSIBILITY_DELIMITER);
            }
            builder.append(service.flattenToString());
        }

        Log.d(TAG, "New list of accessibility services: " + builder);
        Settings.Secure.putString(context.getContentResolver(),
                Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES,
                builder.toString());

        // Set the overall enabled state.
        int desiredState = enable ? ON : OFF;
        Settings.Secure.putInt(context.getContentResolver(), Settings.Secure.ACCESSIBILITY_ENABLED,
                desiredState);
    }
}
