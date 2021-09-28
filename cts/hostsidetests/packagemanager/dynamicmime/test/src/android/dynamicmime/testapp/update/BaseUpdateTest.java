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

package android.dynamicmime.testapp.update;

import static android.dynamicmime.common.Constants.PACKAGE_UPDATE_APP;

import android.dynamicmime.testapp.BaseDynamicMimeTest;
import android.dynamicmime.testapp.assertions.MimeGroupAssertions;
import android.dynamicmime.testapp.commands.MimeGroupCommands;
import android.dynamicmime.testapp.util.Utils;

import org.junit.After;
import org.junit.Before;

import java.io.IOException;

abstract class BaseUpdateTest extends BaseDynamicMimeTest {
    BaseUpdateTest() {
        super(MimeGroupCommands.appWithUpdates(context()),
                MimeGroupAssertions.appWithUpdates(context()));
    }

    @Before
    public void setUp() throws IOException {
        Utils.installApk(installApkPath());
    }

    @After
    @Override
    public void tearDown() {
        super.tearDown();
        Utils.uninstallApp(PACKAGE_UPDATE_APP);
    }

    void updateApp() {
        Utils.updateApp(updateApkPath());
    }

    abstract String installApkPath();
    abstract String updateApkPath();
}
