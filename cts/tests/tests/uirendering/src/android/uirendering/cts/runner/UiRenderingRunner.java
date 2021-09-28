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

package android.uirendering.cts.runner;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.server.wm.settings.SettingsSession;
import android.support.test.uiautomator.UiDevice;
import android.uirendering.cts.util.BitmapDumper;

import androidx.test.runner.AndroidJUnitRunner;

/**
 * TODO: Do some cool stuff we also want like sharing DrawActivity cross-class.
 */
public class UiRenderingRunner extends AndroidJUnitRunner {

    private static class ImmersiveConfirmationSetting extends SettingsSession<String> {
        ImmersiveConfirmationSetting() {
            super(Settings.Secure.getUriFor(
                    Settings.Secure.IMMERSIVE_MODE_CONFIRMATIONS),
                    Settings.Secure::getString, Settings.Secure::putString);
        }
    }

    private ImmersiveConfirmationSetting mSettingsSession;

    @Override
    protected void waitForActivitiesToComplete() {
        // No.
    }

    @Override
    public void onCreate(Bundle arguments) {
        super.onCreate(arguments);

        // Disable immersive clings
        mSettingsSession = new ImmersiveConfirmationSetting();
        mSettingsSession.set("confirmed");

        final UiDevice device = UiDevice.getInstance(this);
        try {
            device.wakeUp();
            device.executeShellCommand("wm dismiss-keyguard");
            getContext().sendBroadcast(new Intent(Intent.ACTION_CLOSE_SYSTEM_DIALOGS));
        } catch (Exception e) {
        }

        BitmapDumper.initialize(this);
    }

    @Override
    public void onDestroy() {
        // Ok now wait if necessary
        super.waitForActivitiesToComplete();

        mSettingsSession.close();

        super.onDestroy();
    }
}
