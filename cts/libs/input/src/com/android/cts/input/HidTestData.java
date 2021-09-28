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

package com.android.cts.input;

import android.view.InputEvent;

import java.util.ArrayList;
import java.util.List;

/**
 * Data class that stores HID test data.
 *
 * There need not be a 1:1 mapping from reports to events. It is possible that some reports may
 * generate more than 1 event (maybe 2 buttons were pressed simultaneously, for example).
 */
public class HidTestData {
    // Name of the test
    public String name;

    // HID reports that are used as input to /dev/uhid
    public List<String> reports = new ArrayList<String>();

    // InputEvent's that are expected to be produced after sending out the reports.
    public List<InputEvent> events = new ArrayList<InputEvent>();
}
