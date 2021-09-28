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

package android.provider.cts;

import static org.junit.Assert.assertEquals;

import android.provider.SearchIndexableResource;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class SearchIndexableResourceTest {

    private static final int RANK = 3;
    private static final int XML_RES_ID = 4;
    private static final String CLASS_NAME = "testClassName";
    private static final int ICON_RES_ID = 5;

    @Test
    public void testConstructor() {
        SearchIndexableResource resource = new SearchIndexableResource(RANK, XML_RES_ID, CLASS_NAME,
                ICON_RES_ID);

        assertEquals(RANK, resource.rank);
        assertEquals(XML_RES_ID, resource.xmlResId);
        assertEquals(CLASS_NAME, resource.className);
        assertEquals(ICON_RES_ID, resource.iconResId);
    }
}
