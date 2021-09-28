/*
 * Copyright (C) 2021 The Android Open Source Project
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

package android.security.cts;

import android.content.Context;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.platform.test.annotations.SecurityTest;
import android.provider.VoicemailContract;
import android.test.AndroidTestCase;
import androidx.test.InstrumentationRegistry;
import static org.junit.Assert.*;

@SecurityTest
public class SQLiteTest extends AndroidTestCase {

    private ContentResolver mResolver;
    private String mPackageName;
    private Context mContext;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mResolver = getContext().getContentResolver();
        mContext = InstrumentationRegistry.getTargetContext();
        mPackageName = mContext.getPackageName();
    }

    /**
     * b/139186193
     */
    @SecurityTest(minPatchLevel = "2019-11")
    public void test_android_cve_2019_2195() {
        Uri uri = VoicemailContract.Voicemails.CONTENT_URI;
        uri = uri.buildUpon().appendQueryParameter("source_package", mPackageName).build();

        try {
            mContext.grantUriPermission(mPackageName, uri,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                            | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        } catch (Exception e) {
            if (e instanceof java.lang.SecurityException) {
                // this suggests com.android.providers.contacts.VoicemailContentProvider
                // does not allow granting of Uri permissions, hence return.
                return;
            }
        }

        try {
            String fileToDump = mContext.createPackageContext("com.android.providers.contacts", 0)
                    .getDatabasePath("contacts2.db").getAbsolutePath();
            try {
                mResolver.query(uri, null, null, null,
                   "_id ASC LIMIT _TOKENIZE('calls(_data,_data,_data,source_package,type) VALUES(''"
                                + fileToDump + "'',?,?,''" + mPackageName + "'',4);',0,'','Z')")
                        .close();
                fail("Vulnerable function exists");
            } catch (android.database.sqlite.SQLiteException e) {
                    // do nothing
            }
        } catch (NameNotFoundException n) {
            // do nothing
        }
    }
}
