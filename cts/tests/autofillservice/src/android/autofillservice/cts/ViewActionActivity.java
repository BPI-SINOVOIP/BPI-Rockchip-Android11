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

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.test.uiautomator.UiObject2;
import android.util.Log;
import android.widget.TextView;

/**
 * Activity that handles VIEW action.
 */
public class ViewActionActivity extends AbstractAutoFillActivity {

    private static ViewActionActivity sInstance;

    private static final String TAG = "ViewActionHandleActivity";
    static final String ID_WELCOME = "welcome";
    static final String DEFAULT_MESSAGE = "Welcome VIEW action handle activity";
    private boolean mHasCustomBackBehavior;

    enum ActivityCustomAction {
        NORMAL_ACTIVITY,
        FAST_FORWARD_ANOTHER_ACTIVITY,
        TAP_BACK_WITHOUT_FINISH
    }

    public ViewActionActivity() {
        sInstance = this;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.welcome_activity);

        final Uri data = getIntent().getData();
        ActivityCustomAction type = ActivityCustomAction.valueOf(data.getSchemeSpecificPart());

        switch (type) {
            case FAST_FORWARD_ANOTHER_ACTIVITY:
                startSecondActivity();
                break;
            case TAP_BACK_WITHOUT_FINISH:
                mHasCustomBackBehavior = true;
                break;
            case NORMAL_ACTIVITY:
            default:
                // no-op
        }

        TextView welcome = (TextView) findViewById(R.id.welcome);
        welcome.setText(DEFAULT_MESSAGE);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();

        Log.v(TAG, "Setting sInstance to null onDestroy()");
        sInstance = null;
    }

    @Override
    public void finish() {
        super.finish();
        mHasCustomBackBehavior = false;
    }

    @Override
    public void onBackPressed() {
        if (mHasCustomBackBehavior) {
            moveTaskToBack(true);
            return;
        }
        super.onBackPressed();
    }

    static void finishIt() {
        if (sInstance != null) {
            sInstance.finish();
        }
    }

    private void startSecondActivity() {
        final Intent intent = new Intent(this, SecondActivity.class)
                .setFlags(Intent.FLAG_ACTIVITY_NO_HISTORY);
        startActivity(intent);
        finish();
    }

    static void assertShowingDefaultMessage(UiBot uiBot) throws Exception {
        final UiObject2 activity = uiBot.assertShownByRelativeId(ID_WELCOME);
        assertWithMessage("wrong text on '%s'", activity).that(activity.getText())
                .isEqualTo(DEFAULT_MESSAGE);
    }
}
