/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.bluetooth.avrcpcontroller;

import android.support.v4.media.session.PlaybackStateCompat;
import android.util.Log;
import android.util.SparseArray;

import java.util.ArrayList;

/*
 * Contains information Player Application Setting extended from BluetootAvrcpPlayerSettings
 */
class PlayerApplicationSettings {
    private static final String TAG = "PlayerApplicationSettings";

    /*
     * Values for SetPlayerApplicationSettings from AVRCP Spec V1.6 Appendix F.
     */
    static final byte EQUALIZER_STATUS = 0x01;
    static final byte REPEAT_STATUS = 0x02;
    static final byte SHUFFLE_STATUS = 0x03;
    static final byte SCAN_STATUS = 0x04;

    private static final byte JNI_EQUALIZER_STATUS_OFF = 0x01;
    private static final byte JNI_EQUALIZER_STATUS_ON = 0x02;

    private static final byte JNI_REPEAT_STATUS_OFF = 0x01;
    private static final byte JNI_REPEAT_STATUS_SINGLE_TRACK_REPEAT = 0x02;
    private static final byte JNI_REPEAT_STATUS_ALL_TRACK_REPEAT = 0x03;
    private static final byte JNI_REPEAT_STATUS_GROUP_REPEAT = 0x04;

    private static final byte JNI_SHUFFLE_STATUS_OFF = 0x01;
    private static final byte JNI_SHUFFLE_STATUS_ALL_TRACK_SHUFFLE = 0x02;
    private static final byte JNI_SHUFFLE_STATUS_GROUP_SHUFFLE = 0x03;

    private static final byte JNI_SCAN_STATUS_OFF = 0x01;
    private static final byte JNI_SCAN_STATUS_ALL_TRACK_SCAN = 0x02;
    private static final byte JNI_SCAN_STATUS_GROUP_SCAN = 0x03;

    private static final byte JNI_STATUS_INVALID = -1;

    /*
     * Hash map of current settings.
     */
    private SparseArray<Integer> mSettings = new SparseArray<>();

    /*
     * Hash map of supported values, a setting should be supported by the remote in order to enable
     * in mSettings.
     */
    private SparseArray<ArrayList<Integer>> mSupportedValues =
            new SparseArray<ArrayList<Integer>>();

    /* Convert from JNI array to Java classes. */
    static PlayerApplicationSettings makeSupportedSettings(byte[] btAvrcpAttributeList) {
        PlayerApplicationSettings newObj = new PlayerApplicationSettings();
        try {
            for (int i = 0; i < btAvrcpAttributeList.length; ) {
                byte attrId = btAvrcpAttributeList[i++];
                byte numSupportedVals = btAvrcpAttributeList[i++];
                ArrayList<Integer> supportedValues = new ArrayList<Integer>();

                for (int j = 0; j < numSupportedVals; j++) {
                    // Yes, keep using i for array indexing.
                    supportedValues.add(
                            mapAttribIdValtoAvrcpPlayerSetting(attrId, btAvrcpAttributeList[i++]));
                }
                newObj.mSupportedValues.put(attrId, supportedValues);
            }
        } catch (ArrayIndexOutOfBoundsException exception) {
            Log.e(TAG, "makeSupportedSettings attributeList index error.");
        }
        return newObj;
    }

    static PlayerApplicationSettings makeSettings(byte[] btAvrcpAttributeList) {
        PlayerApplicationSettings newObj = new PlayerApplicationSettings();
        try {
            for (int i = 0; i < btAvrcpAttributeList.length; ) {
                byte attrId = btAvrcpAttributeList[i++];

                newObj.mSettings.put(attrId,
                        mapAttribIdValtoAvrcpPlayerSetting(attrId, btAvrcpAttributeList[i++]));
            }
        } catch (ArrayIndexOutOfBoundsException exception) {
            Log.e(TAG, "makeSettings JNI_ATTRIButeList index error.");
        }
        return newObj;
    }

    public void setSupport(PlayerApplicationSettings updates) {
        mSettings = updates.mSettings;
        mSupportedValues = updates.mSupportedValues;
    }

    public boolean supportsSetting(int settingType, int settingValue) {
        if (null == mSupportedValues.get(settingType)) return false;
        return mSupportedValues.valueAt(settingType).contains(settingValue);
    }

    public boolean supportsSetting(int settingType) {
        return (null != mSupportedValues.get(settingType));
    }

    public int getSetting(int settingType) {
        if (null == mSettings.get(settingType)) return -1;
        return mSettings.get(settingType);
    }

    // Convert a native Attribute Id/Value pair into the AVRCP equivalent value.
    private static int mapAttribIdValtoAvrcpPlayerSetting(byte attribId, byte attribVal) {
        if (attribId == REPEAT_STATUS) {
            switch (attribVal) {
                case JNI_REPEAT_STATUS_ALL_TRACK_REPEAT:
                    return PlaybackStateCompat.REPEAT_MODE_ALL;
                case JNI_REPEAT_STATUS_GROUP_REPEAT:
                    return PlaybackStateCompat.REPEAT_MODE_GROUP;
                case JNI_REPEAT_STATUS_OFF:
                    return PlaybackStateCompat.REPEAT_MODE_NONE;
                case JNI_REPEAT_STATUS_SINGLE_TRACK_REPEAT:
                    return PlaybackStateCompat.REPEAT_MODE_ONE;
            }
        } else if (attribId == SHUFFLE_STATUS) {
            switch (attribVal) {
                case JNI_SHUFFLE_STATUS_ALL_TRACK_SHUFFLE:
                    return PlaybackStateCompat.SHUFFLE_MODE_ALL;
                case JNI_SHUFFLE_STATUS_GROUP_SHUFFLE:
                    return PlaybackStateCompat.SHUFFLE_MODE_GROUP;
                case JNI_SHUFFLE_STATUS_OFF:
                    return PlaybackStateCompat.SHUFFLE_MODE_NONE;
            }
        }
        return JNI_STATUS_INVALID;
    }

    // Convert an AVRCP Setting/Value pair into the native equivalent value;
    static byte mapAvrcpPlayerSettingstoBTattribVal(int mSetting, int mSettingVal) {
        if (mSetting == REPEAT_STATUS) {
            switch (mSettingVal) {
                case PlaybackStateCompat.REPEAT_MODE_NONE:
                    return JNI_REPEAT_STATUS_OFF;
                case PlaybackStateCompat.REPEAT_MODE_ONE:
                    return JNI_REPEAT_STATUS_SINGLE_TRACK_REPEAT;
                case PlaybackStateCompat.REPEAT_MODE_ALL:
                    return JNI_REPEAT_STATUS_ALL_TRACK_REPEAT;
                case PlaybackStateCompat.REPEAT_MODE_GROUP:
                    return JNI_REPEAT_STATUS_GROUP_REPEAT;
            }
        } else if (mSetting == SHUFFLE_STATUS) {
            switch (mSettingVal) {
                case PlaybackStateCompat.SHUFFLE_MODE_NONE:
                    return JNI_SHUFFLE_STATUS_OFF;
                case PlaybackStateCompat.SHUFFLE_MODE_ALL:
                    return JNI_SHUFFLE_STATUS_ALL_TRACK_SHUFFLE;
                case PlaybackStateCompat.SHUFFLE_MODE_GROUP:
                    return JNI_SHUFFLE_STATUS_GROUP_SHUFFLE;
            }
        }
        return JNI_STATUS_INVALID;
    }
}
