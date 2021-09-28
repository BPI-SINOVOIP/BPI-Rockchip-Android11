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

package com.android.cts.managedprofile;

import static com.android.cts.managedprofile.BaseManagedProfileTest.ADMIN_RECEIVER_COMPONENT;

import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.content.Intent;
import android.os.Bundle;
import android.provider.CalendarContract;
import android.widget.TextView;

public class DummyCrossProfileViewEventActivity extends Activity {

    private DevicePolicyManager mDevicePolicyManager;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.view_event);
        mDevicePolicyManager = getSystemService(DevicePolicyManager.class);
    }

    @Override
    public void onStart() {
        super.onStart();
        // Only set the text when managed profile is getting the
        // ACTION_VIEW_MANAGED_PROFILE_CALENDAR_EVENT intent.
        final Intent intent = getIntent();
        if (intent.getAction() == CalendarContract.ACTION_VIEW_MANAGED_PROFILE_CALENDAR_EVENT
                && isManagedProfile()) {
            TextView textView = findViewById(R.id.view_event_text);
            final Bundle bundle = getIntent().getExtras();
            final String textVeiwString = getViewEventCrossProfileString(
                    (long)bundle.get(CalendarContract.EXTRA_EVENT_ID),
                    (long)bundle.get(CalendarContract.EXTRA_EVENT_BEGIN_TIME),
                    (long)bundle.get(CalendarContract.EXTRA_EVENT_END_TIME),
                    (boolean)bundle.get(CalendarContract.EXTRA_EVENT_ALL_DAY),
                    getIntent().getFlags()
            );
            textView.setText(textVeiwString);
        }
    }

    private String getViewEventCrossProfileString(long eventId, long start, long end,
            boolean allDay, int flags) {
        return String.format("id:%d, start:%d, end:%d, allday:%b, flag:%d", eventId,
                start, end, allDay, flags);
    }

    private boolean isManagedProfile() {
        String adminPackage = ADMIN_RECEIVER_COMPONENT.getPackageName();
        return mDevicePolicyManager.isProfileOwnerApp(adminPackage);
    }
}
