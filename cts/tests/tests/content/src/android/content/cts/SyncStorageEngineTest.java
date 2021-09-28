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

package android.content.cts;

import android.accounts.Account;
import android.content.SyncStatusInfo;
import android.os.Looper;
import android.platform.test.annotations.AppModeFull;
import android.test.AndroidTestCase;
import android.util.AtomicFile;

import com.android.server.content.SyncStorageEngine;

import java.io.File;
import java.io.FileOutputStream;

@AppModeFull(reason = "Sync manager not supported")
public class SyncStorageEngineTest extends AndroidTestCase {
    public void testMalformedAuthority() throws Exception {
        Looper.prepare();
        // Authority id is non integer. It should be discarded by SyncStorageEngine.
        byte[] accountsFileData = ("<?xml version='1.0' encoding='utf-8' standalone='yes' ?>\n"
                + "<accounts>\n"
                + "<listenForTickles user=\"0\" enabled=\"false\" />"
                + "<listenForTickles user=\"1\" enabled=\"true\" />"
                + "<authority id=\"nonint\" user=\"0\" account=\"account1\" type=\"type1\""
                + " authority=\"auth1\" />\n"
                + "</accounts>\n").getBytes();

        File syncDir = getSyncDir();
        syncDir.mkdirs();
        AtomicFile accountInfoFile = new AtomicFile(new File(syncDir, "accounts.xml"));
        FileOutputStream fos = accountInfoFile.startWrite();
        fos.write(accountsFileData);
        accountInfoFile.finishWrite(fos);

        SyncStorageEngine engine = SyncStorageEngine.newTestInstance(getContext());
        Account account = new Account("account1", "type1");
        SyncStorageEngine.EndPoint endPoint = new SyncStorageEngine.EndPoint(account, "auth1", 0);
        SyncStatusInfo info = engine.getStatusByAuthority(endPoint);
        assertNull(info);
    }

    private File getSyncDir() {
        return new File(new File(getContext().getFilesDir(), "system"), "sync");
    }
}
