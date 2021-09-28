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

package android.dynamicmime.testapp.reboot;

import android.dynamicmime.testapp.SingleAppTest;
import android.dynamicmime.testapp.commands.MimeGroupCommands;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.runner.RunWith;

/**
 * Post-reboot part of {@link SingleAppTest}
 *
 * {@link MimeGroupCommands} are disabled during test body execution
 */
@RunWith(AndroidJUnit4.class)
public class PostRebootSingleAppTest extends SingleAppTest {
    private MimeGroupCommands mCommands;

    public PostRebootSingleAppTest() {
        super();
        mCommands = getCommands();
    }

    @Before
    public void setUp() {
        setCommands(MimeGroupCommands.noOp());
    }

    @After
    @Override
    public void tearDown() {
        setCommands(mCommands);
        super.tearDown();
    }
}
