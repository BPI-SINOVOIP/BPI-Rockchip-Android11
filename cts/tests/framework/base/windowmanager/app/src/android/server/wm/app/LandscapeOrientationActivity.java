/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */

package android.server.wm.app;

import static android.server.wm.app.Components.LandscapeOrientationActivity.EXTRA_APP_CONFIG_INFO;
import static android.server.wm.app.Components.LandscapeOrientationActivity.EXTRA_CONFIG_INFO_IN_ON_CREATE;
import static android.server.wm.app.Components.LandscapeOrientationActivity.EXTRA_DISPLAY_REAL_SIZE;
import static android.server.wm.app.Components.LandscapeOrientationActivity.EXTRA_SYSTEM_RESOURCES_CONFIG_INFO;

import android.app.Application;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Point;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.server.wm.CommandSession.ConfigInfo;
import android.view.Display;

public class LandscapeOrientationActivity extends AbstractLifecycleLogActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (!getIntent().hasExtra(EXTRA_CONFIG_INFO_IN_ON_CREATE)) {
            return;
        }
        withTestJournalClient(client -> {
            final Bundle extras = new Bundle();
            final Display display = getDisplay();
            final Point size = new Point();
            display.getRealSize(size);
            extras.putParcelable(EXTRA_CONFIG_INFO_IN_ON_CREATE, new ConfigInfo(this, display));
            extras.putParcelable(EXTRA_DISPLAY_REAL_SIZE, size);

            final Application app = getApplication();
            extras.putParcelable(EXTRA_APP_CONFIG_INFO, new ConfigInfo(app,
                    // The display object is associated with application resources that it has its
                    // own display adjustments.
                    app.getSystemService(DisplayManager.class)
                            .getDisplay(Display.DEFAULT_DISPLAY)));
            extras.putParcelable(EXTRA_SYSTEM_RESOURCES_CONFIG_INFO,
                    new ConfigInfo(Resources.getSystem()));
            client.putExtras(extras);
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        dumpConfigInfo();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        dumpConfigInfo();
    }
}
