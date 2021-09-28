/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.text.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.Until;
import android.text.ClipboardManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test {@link ClipboardManager}.
 */
@RunWith(AndroidJUnit4.class)
public class ClipboardManagerTest {
    private ClipboardManager mClipboardManager;
    private UiDevice mUiDevice;
    private Context mContext;

    @Before
    public void setup() {
        mContext = InstrumentationRegistry.getTargetContext();
        mClipboardManager = (ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
        mUiDevice = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        launchActivity(MockActivity.class);
    }

    @Test
    public void testAccessText() {
        // set the expected value
        CharSequence expected = "test";
        mClipboardManager.setText(expected);
        assertEquals(expected, mClipboardManager.getText());
    }

    @Test
    public void testHasText() {
        mClipboardManager.setText("");
        assertFalse(mClipboardManager.hasText());

        mClipboardManager.setText("test");
        assertTrue(mClipboardManager.hasText());

        mClipboardManager.setText(null);
        assertFalse(mClipboardManager.hasText());
    }

    private void launchActivity(Class<? extends Activity> clazz) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setClassName(mContext.getPackageName(), clazz.getName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivity(intent);
        mUiDevice.wait(Until.hasObject(By.clazz(clazz)), 5000);
    }

}
