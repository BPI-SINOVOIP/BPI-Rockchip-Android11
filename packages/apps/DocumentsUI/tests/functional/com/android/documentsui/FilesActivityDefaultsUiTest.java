/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.documentsui;

import static com.android.documentsui.StubProvider.ROOT_0_ID;
import static com.android.documentsui.StubProvider.ROOT_1_ID;

import android.os.RemoteException;

import androidx.recyclerview.R;
import androidx.test.filters.LargeTest;

import com.android.documentsui.base.RootInfo;
import com.android.documentsui.files.FilesActivity;
import com.android.documentsui.filters.HugeLongTest;

@LargeTest
public class FilesActivityDefaultsUiTest extends ActivityTest<FilesActivity> {

    public FilesActivityDefaultsUiTest() {
        super(FilesActivity.class);
    }

    @Override
    protected void initTestFiles() throws RemoteException {
        // Overriding to init with no items in test roots
    }

    @Override
    protected RootInfo getInitialRoot() {
        return null;  // test the default, unaffected state of the app.
    }

    @HugeLongTest
    public void testNavigate_FromEmptyDirectory() throws Exception {
        device.waitForIdle();

        bots.roots.openRoot(rootDir0.title);

        String msg = String.valueOf(context.getString(R.string.empty));
        bots.directory.assertPlaceholderMessageText(msg);

        // Check to make sure back button is properly handled by non-Doc type DocHolders
        device.pressBack();
    }

    @HugeLongTest
    public void testDefaultRoots() throws Exception {
        device.waitForIdle();

        // Should also have Drive, but that requires pre-configuration of devices
        // We omit for now.
        bots.roots.assertRootsPresent(
                "Images",
                "Videos",
                "Audio",
                "Downloads",
                ROOT_0_ID,
                ROOT_1_ID);
    }
}
