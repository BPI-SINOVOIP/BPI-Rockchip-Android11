/**
 * Copyright (c) 2019 The Android Open Source Project
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

package com.android.car.secondaryhome.launcher;

import android.os.Bundle;
import android.util.Log;
import android.widget.ImageButton;

import androidx.appcompat.app.AppCompatActivity;

import com.android.car.secondaryhome.R;

/**
 * Main launcher activity.
 * It contains a navigation bar with BACK and HOME buttons.
 */
public final class LauncherActivity extends AppCompatActivity {
    public static final String APP_FRAGMENT_TAG = "app";
    public static final String HOME_FRAGMENT_TAG = "home";
    public static final String NOTIFICATION_FRAGMENT_TAG = "notification";
    private static final String TAG = "SecHome.LauncherActivity";

    private final AppFragment mAppFragment = new AppFragment();
    private final HomeFragment mHomeFragment = new HomeFragment();
    private final NotificationFragment mNotificationFragment =
            new NotificationFragment();

    private String mLastFragment = HOME_FRAGMENT_TAG;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_main);

        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "Secondary home activity created...");
        }

        getSupportFragmentManager().beginTransaction()
            .add(R.id.container, mAppFragment, APP_FRAGMENT_TAG)
            .commit();

        getSupportFragmentManager().beginTransaction()
            .add(R.id.container, mHomeFragment, HOME_FRAGMENT_TAG)
            .commit();

        getSupportFragmentManager().beginTransaction()
            .add(R.id.container, mNotificationFragment, NOTIFICATION_FRAGMENT_TAG)
            .commit();

        ImageButton backButton = findViewById(R.id.nav_back);
        backButton.setOnClickListener(view -> onBackButtonPressed());
        ImageButton homeButton = findViewById(R.id.nav_home);
        homeButton.setOnClickListener(view -> navigateHome());
        ImageButton notificationButton = findViewById(R.id.nav_notification);
        notificationButton.setOnClickListener(view -> toggleNotification());
    }

    @Override
    protected void onStart() {
        super.onStart();
        navigateHome();
    }

    private void onBackButtonPressed() {
        // When BACK is pressed, if HomeFragment is shown
        // and AppFragment's has valid and non-empty task stack, navigate to AppFragment;
        // if AppFragment is shown, pop from stack on AppFragment;
        // if NotificationFragment is shown, navigate to previous fragment.
        if (mHomeFragment.isVisible()
                && mAppFragment.getTaskStackId() != AppFragment.INVALID_TASK_STACK_ID) {
            navigateApp();
        } else if (mAppFragment.isVisible()) {
            mAppFragment.onBackButtonPressed();
        } else if (mNotificationFragment.isVisible()) {
            toggleNotification();
        }
    }

    public void navigateHome() {
        getSupportFragmentManager().beginTransaction()
            .show(mHomeFragment)
            .commit();
        getSupportFragmentManager().beginTransaction()
            .hide(mAppFragment)
            .commit();
        getSupportFragmentManager().beginTransaction()
            .hide(mNotificationFragment)
            .commit();

        mLastFragment = HOME_FRAGMENT_TAG;
    }

    public void navigateApp() {
        getSupportFragmentManager().beginTransaction()
            .hide(mHomeFragment)
            .commit();
        getSupportFragmentManager().beginTransaction()
            .show(mAppFragment)
            .commit();
        getSupportFragmentManager().beginTransaction()
            .hide(mNotificationFragment)
            .commit();

        mLastFragment = APP_FRAGMENT_TAG;
    }

    public void toggleNotification() {
        if (!mNotificationFragment.isVisible()) {
            getSupportFragmentManager().beginTransaction()
                .hide(mHomeFragment)
                .commit();
            getSupportFragmentManager().beginTransaction()
                .hide(mAppFragment)
                .commit();
            getSupportFragmentManager().beginTransaction()
                .show(mNotificationFragment)
                .commit();
        } else if (mLastFragment.equals(HOME_FRAGMENT_TAG)) {
            navigateHome();
        } else {
            navigateApp();
        }
    }
}
