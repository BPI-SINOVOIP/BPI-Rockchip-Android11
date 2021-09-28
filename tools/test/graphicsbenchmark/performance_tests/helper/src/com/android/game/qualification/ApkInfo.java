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

import java.util.List;

public class ApkInfo {
    public static final String APK_LIST_LOCATION =
        "/sdcard/Android/data/com.android.game.qualification.device/apk-info.xml";

    public static class Argument {
        public enum Type {
            STRING,
            BOOLEAN,
            BYTE,
            INT,
            LONG,
            FLOAT,
            DOUBLE,
        };

        private String mKey;
        private String mValue;
        private Type mType;

        public Argument(String key, String value, Type type) {
            mKey = key;
            mValue = value;
            mType = type;
        }

        public String getKey() {
            return mKey;
        }

        public String getValue() {
            return mValue;
        }

        public Type getType() {
            return mType;
        }
    }

    private String mName;
    private String mFileName;
    private String mPackageName;
    private String mActivityName;
    private String mLayerName;
    private String mScript;
    private List<Argument> mArgs;
    private int mLoadTime;
    private int mRunTime;
    private boolean mExpectIntents;

    public ApkInfo(
            String name,
            String fileName,
            String packageName,
            String activityName,
            String layerName,
            String script,
            List<Argument> args,
            int loadTime,
            int runTime,
            boolean expectIntents) {
        mActivityName = activityName;
        checkNotNull(name, Field.NAME.getTag());
        checkNotNull(fileName, Field.FILE_NAME.getTag());
        checkNotNull(packageName, Field.PACKAGE_NAME.getTag());
        checkNotNull(layerName, Field.LAYER_NAME.getTag());

        this.mName = name;
        this.mFileName = fileName;
        this.mPackageName = packageName;
        this.mLayerName = layerName;
        this.mScript = script;
        this.mArgs = args;
        this.mLoadTime = loadTime;
        this.mRunTime = runTime;
        this.mExpectIntents = expectIntents;
    }

    /** Name of the test */
    public String getName() {
        return mName;
    }

    /** Filename of the APK */
    public String getFileName() {
        return mFileName;
    }

    /** Package name of the app */
    public String getPackageName() {
        return mPackageName;
    }

    /** (Optional) name of the activity to launch if different from the default of the package. */
    public String getActivityName() {
        return mActivityName;
    }

    /** Name of the layer to collect metrics from */
    public String getLayerName() {
        return mLayerName;
    }

    /** (Optional) Shell command to be executed before the installation of the APK */
    public String getScript() {
        return mScript;
    }

    /** (Optional) Arguments supplied to the Intent used to start the app */
    public List<Argument> getArgs() {
        return mArgs;
    }

    /**
     * (Optional) Duration (in milliseconds) the app will be run for before it is terminated, prior
     * to producing a START_LOOP intent. [default: 10000]
     */
    public int getLoadTime() {
        return mLoadTime;
    }

    /**
     * (Optional) Duration (in milliseconds) the app will be run for after the first START_LOOP
     * intent is received. [default: 10000]
     */
    public int getRunTime() {
        return mRunTime;
    }

    /**
     * Whether the app integrates properly with the harness via intents.
     *
     * If true, then we require that the app send its first START_LOOP intent within getLoadTime(),
     * and run it for an additional getRunTime() milliseconds after the first intent is received.
     *
     * Otherwise, we will run the app for getLoadTime() + getRunTime(), and produce a synthetic
     * "finished loading" marker after getLoadTime() milliseconds.
     */
    public boolean getExpectIntents() {
        return mExpectIntents;
    }

    private void checkNotNull(Object value, String fieldName) {
        if (value == null) {
            throw new IllegalArgumentException(
                    "Failed to parse apk info.  Required field '" + fieldName + "' is missing.");
        }
    }
}
