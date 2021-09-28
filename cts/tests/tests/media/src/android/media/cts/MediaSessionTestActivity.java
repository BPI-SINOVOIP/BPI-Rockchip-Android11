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
 * limitations under the License
 */

package android.media.cts;

import static org.junit.Assert.fail;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.media.session.MediaController;
import android.media.session.MediaSession;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

/**
 * Activity for testing foreground activity behavior with the
 * {@link android.media.session.MediaSession}.
 */
public class MediaSessionTestActivity extends Activity {
    public static final String KEY_SESSION_TOKEN = "KEY_SESSION_TOKEN";
    private static final String TAG = "MediaSessionTestActivity";

    private boolean mDeviceLocked;
    private boolean mResumed;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Wake up device.
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        setTurnScreenOn(true);

        // Unlock device which is previously locked by power button press.
        // This is required even when the screen lock is set to 'None'.
        setShowWhenLocked(true);

        KeyguardManager keyguardManager =
                (KeyguardManager) getSystemService(Context.KEYGUARD_SERVICE);
        // KeyguardManager can be null for the instant mode.
        if (keyguardManager == null) {
            Log.i(TAG, "Unable to get KeyguardManager. Probably in the instant mode.");
        } else if (keyguardManager.isKeyguardLocked()) {
            Log.i(TAG, "Device is locked. Try unlocking and bring activity foreground.");
            mDeviceLocked = true;
            // Note: CTS requires 'no lock pattern or password is set on the device'.
            // However, try to dismiss keyguard for convenience.
            keyguardManager.requestDismissKeyguard(this,
                    new KeyguardManager.KeyguardDismissCallback() {
                        @Override
                        public void onDismissError() {
                            finish();
                        }

                        @Override
                        public void onDismissCancelled() {
                            finish();
                        }

                        @Override
                        public void onDismissSucceeded() {
                            mDeviceLocked = false;
                            setMediaControllerIfInForeground();
                        }
                    });
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        mResumed = true;
        setMediaControllerIfInForeground();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mResumed = false;
        setMediaController(null);
    }

    private void setMediaControllerIfInForeground() {
        if (mDeviceLocked || !mResumed) {
            return;
        }
        MediaSession.Token token = getIntent().getParcelableExtra(KEY_SESSION_TOKEN);
        if (token != null) {
            MediaController controller = new MediaController(this, token);
            setMediaController(controller);
        }
    }
}
