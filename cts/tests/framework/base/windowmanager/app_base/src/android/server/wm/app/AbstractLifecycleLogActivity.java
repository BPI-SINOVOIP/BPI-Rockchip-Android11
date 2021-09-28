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

import static android.server.wm.app.Components.TestActivity.EXTRA_CONFIGURATION;

import android.content.res.Configuration;
import android.os.Bundle;
import android.server.wm.CommandSession.BasicTestActivity;
import android.server.wm.CommandSession.ConfigInfo;
import android.util.Log;

public abstract class AbstractLifecycleLogActivity extends BasicTestActivity {

    @Override
    protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        Log.i(getTag(), "onCreate");
    }

    @Override
    protected void onStart() {
        super.onStart();
        Log.i(getTag(), "onStart");
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(getTag(), "onResume");
    }

    @Override
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        super.onTopResumedActivityChanged(isTopResumedActivity);
        Log.i(getTag(), "onTopResumedActivityChanged: " + isTopResumedActivity);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        Log.i(getTag(), "onConfigurationChanged");
    }

    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode, Configuration newConfig) {
        super.onMultiWindowModeChanged(isInMultiWindowMode, newConfig);
        Log.i(getTag(), "onMultiWindowModeChanged");
    }

    @Override
    public void onPictureInPictureModeChanged(boolean isInPictureInPictureMode,
            Configuration newConfig) {
        super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig);
        Log.i(getTag(), "onPictureInPictureModeChanged");
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(getTag(), "onPause");
    }

    @Override
    protected void onStop() {
        super.onStop();
        Log.i(getTag(), "onStop");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(getTag(), "onDestroy");
    }

    @Override
    protected void onUserLeaveHint() {
        super.onUserLeaveHint();
        Log.i(getTag(), "onUserLeaveHint");
    }

    protected final String getTag() {
        return getClass().getSimpleName();
    }

    protected void dumpConfiguration(Configuration config) {
        Log.i(getTag(), "Configuration: " + config);
        withTestJournalClient(client -> {
            final Bundle extras = new Bundle();
            extras.putParcelable(EXTRA_CONFIGURATION, config);
            client.putExtras(extras);
        });
    }

    protected void dumpConfigInfo() {
        // Here dumps when idle because the {@link ConfigInfo} includes some information (display
        // related) got from the attached decor view (after resume). Also if there are several
        // incoming lifecycle callbacks in a short time, it prefers to dump in a stable state.
        runWhenIdle(() -> withTestJournalClient(client -> {
            final ConfigInfo configInfo = getConfigInfo();
            Log.i(getTag(), configInfo.toString());
            client.setLastConfigInfo(configInfo);
        }));
    }
}
