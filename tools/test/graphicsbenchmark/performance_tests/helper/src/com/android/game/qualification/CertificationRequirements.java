/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.game.qualification;

import com.android.game.qualification.GameCoreConfigurationXmlParser.Field;

public class CertificationRequirements {
    private String mName;
    private float mFrameTime;
    private float mJankRate;
    private int mLoadTime;

    public CertificationRequirements(String name, float frameTime, float jankRate, int loadTime) {
        mName = name;
        mFrameTime = frameTime;
        mJankRate = jankRate;
        mLoadTime = loadTime;
        checkNotNull(name, Field.NAME.getTag());
    }

    /**
     * Name of the certification requirements.
     *
     * Must match a corresponding apk info.
     */
    public String getName() {
        return mName;
    }

    /**
     * (Optional) Target frame time (in milliseconds) of the APK. [default: 16.666]
     */
    public float getFrameTime() {
        return mFrameTime;
    }

    /**
     * (Optional) Maximum ratio of frames the APK can miss before failing certification.
     * [default: 0.0]
     */
    public float getJankRate() {
        return mJankRate;
    }

    /**
     * (Optional) Maximum load time in milliseconds. Load time of -1 implies, it is not considered
     * for certification. [default: -1]
     */
    public int getLoadTime() {
        return mLoadTime;
    }

    private void checkNotNull(Object value, String fieldName) {
        if (value == null) {
            throw new IllegalArgumentException(
                    "Failed to parse certfication requirements.  Required field '" + fieldName
                            + "' is missing.");
        }
    }
}
