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

package com.android.bluetooth.map;

import static org.mockito.Mockito.*;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class BluetoothMapbMessageMimeTest {
    private static final String TAG = BluetoothMapbMessageMimeTest.class.getSimpleName();

    @Test
    public void testParseNullMsgPart_NoExceptionsThrown() {
        BluetoothMapbMessageMime bMessageMime = new BluetoothMapbMessageMime();
        bMessageMime.parseMsgPart(null);
    }
}
