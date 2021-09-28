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

package android.content.pm.cts;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.ProviderInfoList;
import android.os.Parcel;
import android.test.AndroidTestCase;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class ProviderInfoListTest extends AndroidTestCase {
    private static final String PACKAGE_NAME = "android.content.cts";

    public void testApplicationInfoSquashed() throws Exception {
        final PackageManager pm = getContext().getPackageManager();
        final PackageInfo pi = pm.getPackageInfo(PACKAGE_NAME,
                PackageManager.GET_PROVIDERS | PackageManager.GET_UNINSTALLED_PACKAGES);

        // Make sure the package contains more than 1 providers.
        assertNotNull(pi);
        assertNotNull(pi.providers);
        assertTrue(pi.providers.length > 1);

        final List<ProviderInfo> providers = new ArrayList<>();
        Collections.addAll(providers, pi.providers);

        // Create a target list.
        final ProviderInfoList source = ProviderInfoList.fromList(providers);

        // Parcel it and uparcel
        final Parcel p = Parcel.obtain();
        source.writeToParcel(p, 0);
        p.setDataPosition(0);
        final ProviderInfoList dest = ProviderInfoList.CREATOR.createFromParcel(p);
        p.recycle();

        final List<ProviderInfo> destList = dest.getList();
        assertEquals(source.getList().size(), destList.size());
        for (int i = 0; i < destList.size(); i++) {
            assertEquals(destList.get(0).applicationInfo, destList.get(i).applicationInfo);
        }
    }
}
