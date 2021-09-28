/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.internal.statusbar;

import static com.google.common.truth.Truth.assertThat;

import android.os.Binder;
import android.os.Parcel;
import android.os.UserHandle;
import android.util.ArrayMap;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;

import com.android.internal.view.AppearanceRegion;

import org.junit.Test;
import org.junit.runner.RunWith;


@RunWith(AndroidJUnit4.class)
@SmallTest
public class RegisterStatusBarResultTest {

    /**
     * Test {@link RegisterStatusBarResult} can be stored then restored with {@link Parcel}.
     */
    @Test
    public void testParcelable() {
        final String dumyIconKey = "dummyIcon1";
        final ArrayMap<String, StatusBarIcon> iconMap = new ArrayMap<>();
        iconMap.put(dumyIconKey, new StatusBarIcon("com.android.internal.statusbar.test",
                UserHandle.of(100), 123, 1, 2, "dummyIconDescription"));

        final RegisterStatusBarResult original = new RegisterStatusBarResult(iconMap,
                0x2 /* disabledFlags1 */,
                0x4 /* appearance */,
                new AppearanceRegion[0] /* appearanceRegions */,
                0x8 /* imeWindowVis */,
                0x10 /* imeBackDisposition */,
                false /* showImeSwitcher */,
                0x20 /* disabledFlags2 */,
                new Binder() /* imeToken */,
                true /* navbarColorManagedByIme */,
                true /* appFullscreen */,
                true /* appImmersive */,
                new int[0] /* transientBarTypes */);

        final RegisterStatusBarResult copy = clone(original);

        assertThat(copy.mIcons).hasSize(original.mIcons.size());
        // We already test that StatusBarIcon is Parcelable.  Only check StatusBarIcon.user here.
        assertThat(copy.mIcons.get(dumyIconKey).user)
                .isEqualTo(original.mIcons.get(dumyIconKey).user);

        assertThat(copy.mDisabledFlags1).isEqualTo(original.mDisabledFlags1);
        assertThat(copy.mAppearance).isEqualTo(original.mAppearance);
        assertThat(copy.mAppearanceRegions).isEqualTo(original.mAppearanceRegions);
        assertThat(copy.mImeWindowVis).isEqualTo(original.mImeWindowVis);
        assertThat(copy.mImeBackDisposition).isEqualTo(original.mImeBackDisposition);
        assertThat(copy.mShowImeSwitcher).isEqualTo(original.mShowImeSwitcher);
        assertThat(copy.mDisabledFlags2).isEqualTo(original.mDisabledFlags2);
        assertThat(copy.mImeToken).isSameAs(original.mImeToken);
        assertThat(copy.mNavbarColorManagedByIme).isEqualTo(original.mNavbarColorManagedByIme);
        assertThat(copy.mAppFullscreen).isEqualTo(original.mAppFullscreen);
        assertThat(copy.mAppImmersive).isEqualTo(original.mAppImmersive);
        assertThat(copy.mTransientBarTypes).isEqualTo(original.mTransientBarTypes);
    }

    private RegisterStatusBarResult clone(RegisterStatusBarResult original) {
        Parcel parcel = null;
        try {
            parcel = Parcel.obtain();
            original.writeToParcel(parcel, 0);
            parcel.setDataPosition(0);
            return RegisterStatusBarResult.CREATOR.createFromParcel(parcel);
        } finally {
            if (parcel != null) {
                parcel.recycle();
            }
        }
    }
}
