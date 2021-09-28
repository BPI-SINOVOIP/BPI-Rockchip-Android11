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
package android.incrementalinstall.incrementaltestapp;

import static android.incrementalinstall.common.Consts.COMPONENT_STATUS_KEY;
import static android.incrementalinstall.common.Consts.INCREMENTAL_TEST_APP_STATUS_RECEIVER_ACTION;
import static android.incrementalinstall.common.Consts.TARGET_COMPONENT_KEY;

import android.app.Activity;
import android.content.Intent;
import android.incrementalinstall.common.Consts;
import android.os.Bundle;

/** A second implementation of MainActivity to verify version updates. */
public class MainActivity extends Activity {

    @Override
    public void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        for (String component : Consts.SupportedComponents.getAllComponents()) {
            broadcastStatus(component,
                    component.equals(Consts.SupportedComponents.ON_CREATE_COMPONENT_2));
        }
    }

    private void broadcastStatus(String component, boolean status) {
        Intent intent = new Intent(INCREMENTAL_TEST_APP_STATUS_RECEIVER_ACTION);
        intent.putExtra(TARGET_COMPONENT_KEY, component);
        intent.putExtra(COMPONENT_STATUS_KEY, status);
        sendBroadcast(intent);
    }
}
