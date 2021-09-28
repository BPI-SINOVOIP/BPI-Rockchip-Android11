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

package android.inputmethodservice.cts.devicetest;

import static org.junit.Assert.assertEquals;

import android.inputmethodservice.cts.ime.CtsBaseInputMethod;
import android.os.Bundle;
import android.os.Process;
import android.os.SystemClock;
import android.os.UserHandle;
import android.text.TextUtils;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputConnectionWrapper;
import android.widget.EditText;
import android.widget.LinearLayout;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.BlockingDeque;
import java.util.concurrent.LinkedBlockingDeque;
import java.util.concurrent.TimeUnit;

/**
 * Provides test scenarios for multi-user scenario.
 */
@RunWith(AndroidJUnit4.class)
public class MultiUserDeviceTest {

    private static final long TIMEOUT_MILLISEC = TimeUnit.SECONDS.toMillis(10);

    /**
     * A special {@link InputConnection} to receive {@link UserHandle} from
     * {@link CtsBaseInputMethod} via {@link InputConnection#performPrivateCommand(String, Bundle)}.
     */
    private static final class ReplyReceivingInputConnection extends InputConnectionWrapper {
        private final String mSessionKey;
        private final BlockingDeque<UserHandle> mResultQueue;

        ReplyReceivingInputConnection(InputConnection target, String sessionKey,
                BlockingDeque<UserHandle> resultQueue) {
            super(target, false);
            mSessionKey = sessionKey;
            mResultQueue = resultQueue;
        }

        /**
         * {@inheritDoc}
         */
        @Override
        public boolean performPrivateCommand(String action, Bundle data) {
            if (TextUtils.equals(CtsBaseInputMethod.ACTION_KEY_REPLY_USER_HANDLE, action)
                    && TextUtils.equals(mSessionKey, data.getString(
                            CtsBaseInputMethod.BUNDLE_KEY_REPLY_USER_HANDLE_SESSION_ID))) {
                final UserHandle userHandle =
                        data.getParcelable(CtsBaseInputMethod.BUNDLE_KEY_REPLY_USER_HANDLE);
                if (userHandle != null) {
                    mResultQueue.add(userHandle);
                }
                return true;
            }
            return super.performPrivateCommand(action, data);
        }
    }

    /** Test to check if the app is connecting to the IME that runs under the same user ID. */
    @Test
    public void testConnectingToTheSameUserIme() throws Exception {
        final String sessionId = Long.toString(SystemClock.elapsedRealtimeNanos());
        final BlockingDeque<UserHandle> resultQueue = new LinkedBlockingDeque<>();

        TestActivity.startSync(activity -> {
            final LinearLayout layout = new LinearLayout(activity);
            layout.setOrientation(LinearLayout.VERTICAL);
            final EditText editText = new EditText(activity) {
                @Override
                public InputConnection onCreateInputConnection(EditorInfo editorInfo) {
                    final InputConnection original = super.onCreateInputConnection(editorInfo);
                    if (editorInfo.extras == null) {
                        editorInfo.extras = new Bundle();
                    }
                    editorInfo.extras.putString(
                            CtsBaseInputMethod.EDITOR_INFO_KEY_REPLY_USER_HANDLE_SESSION_ID,
                            sessionId);
                    return new ReplyReceivingInputConnection(original, sessionId, resultQueue);
                }
            };
            editText.requestFocus();
            layout.addView(editText);
            return layout;
        });

        final UserHandle result = resultQueue.poll(TIMEOUT_MILLISEC, TimeUnit.MILLISECONDS);
        assertEquals(Process.myUserHandle(), result);
    }
}
