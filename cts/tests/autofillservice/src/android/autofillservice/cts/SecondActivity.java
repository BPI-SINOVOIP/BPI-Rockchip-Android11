/*
 * Copyright (C) 2019 The Android Open Source Project
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
package android.autofillservice.cts;

import static com.google.common.truth.Truth.assertWithMessage;

import android.os.Bundle;
import android.support.test.uiautomator.UiObject2;
import android.util.Log;
import android.widget.TextView;

/**
 * Activity that is used to test restored mechanism will work while running below steps:
 * 1. Taps span on the save UI to start the ViewActionActivity.
 * 2. Launches the SecondActivity and immediately finish the ViewActionActivity.
 * 3. Presses back key on the SecondActivity.
 * The expected that the save UI should have been restored.
 */
public class SecondActivity extends AbstractAutoFillActivity {

    private static SecondActivity sInstance;

    private static final String TAG = "SecondActivity";
    static final String ID_WELCOME = "welcome";
    static final String DEFAULT_MESSAGE = "Welcome second activity";

    public SecondActivity() {
        sInstance = this;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.welcome_activity);

        TextView welcome = (TextView) findViewById(R.id.welcome);
        welcome.setText(DEFAULT_MESSAGE);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        Log.v(TAG, "Setting sInstance to null onDestroy()");
        sInstance = null;
    }

    static void finishIt() {
        if (sInstance != null) {
            sInstance.finish();
        }
    }

    static void assertShowingDefaultMessage(UiBot uiBot) throws Exception {
        final UiObject2 activity = uiBot.assertShownByRelativeId(ID_WELCOME);
        assertWithMessage("wrong text on '%s'", activity).that(activity.getText())
                .isEqualTo(DEFAULT_MESSAGE);
    }
}
