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

package android.quickaccesswallet.cts;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.Icon;

import com.android.compatibility.common.util.BitmapUtils;

class TestUtils {

    static void compareIcons(Context context, Icon icon1, Icon icon2) {
        if (icon1 == null && icon2 == null) {
            return;
        }
        assertThat(icon1).isNotNull();
        assertThat(icon2).isNotNull();
        Drawable drawable1 = icon1.loadDrawable(context);
        Drawable drawable2 = icon2.loadDrawable(context);
        if (drawable1 instanceof BitmapDrawable) {
            assertThat(drawable2).isInstanceOf(BitmapDrawable.class);
            Bitmap bitmap1 = ((BitmapDrawable) drawable1).getBitmap();
            Bitmap bitmap2 = ((BitmapDrawable) drawable2).getBitmap();
            assertThat(BitmapUtils.compareBitmaps(bitmap1, bitmap2)).isTrue();
        } else {
            assertThat(icon1.getResId()).isEqualTo(icon2.getResId());
        }
    }
}
