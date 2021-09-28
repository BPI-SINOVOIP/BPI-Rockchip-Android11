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

package android.provider.cts.media;

import static android.content.pm.PackageManager.GET_META_DATA;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ComponentInfo;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.provider.apps.cts.gallerytestapp.ReviewActivity;
import android.provider.apps.cts.gallerytestapp.ReviewPrewarmService;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import com.android.internal.util.ArrayUtils;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

@RunWith(AndroidJUnit4.class)
public class MediaStore_MetadataKeysTest {
    private static final String TEST_PACKAGE_NAME = "android.provider.apps.cts.gallerytestapp";
    private static final String TEST_ACTIVITY_NAME = ReviewActivity.class.getName();
    private static final String TEST_SERVICE_NAME = ReviewPrewarmService.class.getName();

    private Context mContext = InstrumentationRegistry.getTargetContext();

    @Test
    public void reviewGalleryPrewarmService() {
        Intent reviewIntent = new Intent(MediaStore.ACTION_REVIEW)
                .setData(Uri.fromParts("content", "data", null));
        List<ResolveInfo> infos = mContext.getPackageManager().queryIntentActivities(reviewIntent,
                GET_META_DATA);
        assertFalse("No matches found for ACTION_REVIEW", ArrayUtils.isEmpty(infos));
        Bundle metaData = getTestActivityMetadata(infos);
        assertNotNull("No meta data retuned", metaData);
        assertEquals(TEST_SERVICE_NAME,
                metaData.getString(MediaStore.META_DATA_REVIEW_GALLERY_PREWARM_SERVICE));
    }

    private static Bundle getTestActivityMetadata(List<ResolveInfo> infos) {
        for (ResolveInfo resolveInfo : infos) {
            ComponentInfo componentInfo = resolveInfo.getComponentInfo();
            if (!TEST_PACKAGE_NAME.equals(componentInfo.packageName)) {
                continue;
            }
            if (componentInfo instanceof ActivityInfo) {
                ActivityInfo activityInfo = (ActivityInfo) componentInfo;
                if (TEST_ACTIVITY_NAME.equals(activityInfo.name)) {
                    return activityInfo.metaData;
                }
            }
        }
        fail(TEST_ACTIVITY_NAME + " not present in the list of matching ResolveInfos");
        return null;
    }
}
