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

package android.server.wm.jetpack;

import static android.server.wm.jetpack.JetpackExtensionTestBase.getActivityWindowToken;

import static com.google.common.truth.Truth.assertThat;

import android.os.Bundle;
import android.os.IBinder;
import android.server.wm.jetpack.wrapper.TestInterfaceCompat;
import android.server.wm.jetpack.wrapper.TestWindowLayoutInfo;
import android.view.View;

import androidx.annotation.Nullable;

/**
 * Activity that can verify the return value of
 * {@link android.server.wm.jetpack.wrapper.TestInterfaceCompat#getWindowLayoutInfo(IBinder)} on
 * activity's different states.
 */
public class TestGetWindowLayoutInfoActivity extends TestActivity {

    private TestInterfaceCompat mExtension;
    @Nullable private TestWindowLayoutInfo prevWindowLayoutInfo;

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mExtension = ExtensionUtils.getInterfaceCompat(this);
        assertCorrectWindowLayoutInfoOrException(true /* isOkToThrowException */);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();
        assertCorrectWindowLayoutInfoOrException(true /* isOkToThrowException */);
    }

    @Override
    protected void onResume() {
        super.onResume();
        assertCorrectWindowLayoutInfoOrException(true /* isOkToThrowException */);
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        super.onLayoutChange(v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom);
        assertCorrectWindowLayoutInfoOrException(false /* isOkToThrowException */);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    private void assertCorrectWindowLayoutInfoOrException(boolean isOkToThrowException) {
        IBinder windowToken = getActivityWindowToken(this);
        if (windowToken == null) {
            return;
        }

        TestWindowLayoutInfo windowLayoutInfo = null;
        try {
            windowLayoutInfo = mExtension.getWindowLayoutInfo(windowToken);
        } catch (Exception e) {
            assertThat(isOkToThrowException).isTrue();
        }

        if (prevWindowLayoutInfo == null) {
            prevWindowLayoutInfo = windowLayoutInfo;
        } else {
            assertThat(windowLayoutInfo).isEqualTo(prevWindowLayoutInfo);
        }
    }
}