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

package android.security.cts;

import android.content.res.AssetManager;
import android.content.res.XmlResourceParser;
import android.platform.test.annotations.SecurityTest;

import com.android.compatibility.common.util.CtsAndroidTestCase;

@SecurityTest
public class AssetManagerTest extends CtsAndroidTestCase {

    // b/144028297
    @SecurityTest(minPatchLevel = "2020-04")
    public void testCloseThenFinalize() throws Exception {
        final XmlResourceParser[] parser = {null};
        final AssetManager[] assetManager = {AssetManager.class.newInstance()};
        parser[0] = assetManager[0].openXmlResourceParser(
                "res/xml/audio_assets.xml");
        assetManager[0].close();
        assetManager[0] = null;

        Runtime.getRuntime().gc();
        Runtime.getRuntime().gc();
        Runtime.getRuntime().runFinalization();

        parser[0] = null;

        Runtime.getRuntime().gc();
        Runtime.getRuntime().gc();
        Runtime.getRuntime().runFinalization();
    }
}
