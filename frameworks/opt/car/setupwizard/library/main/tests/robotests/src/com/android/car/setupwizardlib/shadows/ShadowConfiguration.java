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

package com.android.car.setupwizardlib.shadows;

import android.content.res.Configuration;
import android.view.View;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import java.util.Locale;

/**
 * Shadow class for {@link Configuration}.
 */
@Implements(Configuration.class)
public class ShadowConfiguration {

    private static final Locale HEBREW_LOCALE = new Locale("iw", "IL");
    private int mLayoutDir = View.LAYOUT_DIRECTION_LTR;

    /**
     * Set the layout direction from a {@link Locale}.
     */
    @Implementation
    public void setLayoutDirection(Locale locale) {
        if (locale.getLanguage().equals(HEBREW_LOCALE.getLanguage())) {
            mLayoutDir = View.LAYOUT_DIRECTION_RTL;
        }
    }

    /** Returs the layout direction */
    @Implementation
    public int getLayoutDirection() {
        return mLayoutDir;
    }
}
