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

package android.dynamicmime.common;

public class Constants {
    public static final String ACTION_REQUEST = "android.dynamicmime.UPDATE_MIME_GROUP_REQUEST";
    public static final String ACTION_RESPONSE = "android.dynamicmime.UPDATE_MIME_GROUP_RESPONSE";

    public static final String EXTRA_GROUP = "EXTRA_GROUP";
    public static final String EXTRA_MIMES = "EXTRA_MIME";
    public static final String EXTRA_REQUEST = "EXTRA_REQUEST";
    public static final String EXTRA_RESPONSE = "EXTRA_RESPONSE";

    public static final String GROUP_FIRST = "group_first";
    public static final String GROUP_SECOND = "group_second";
    public static final String GROUP_THIRD = "group_third";

    public static final String ALIAS_BOTH_GROUPS = "group_both";
    public static final String ALIAS_BOTH_GROUPS_AND_TYPE = "groups_and_type";

    public static final int REQUEST_SET = 1;
    public static final int REQUEST_GET = 2;

    public static final String MIME_TEXT_PLAIN = "text/plain";
    public static final String MIME_TEXT_XML = "text/xml";
    public static final String MIME_TEXT_ANY = "text/*";
    public static final String MIME_IMAGE_PNG = "image/png";
    public static final String MIME_IMAGE_JPEG = "image/jpeg";
    public static final String MIME_IMAGE_ANY = "image/*";
    public static final String MIME_ANY = "*/*";

    public static final String PACKAGE_TEST_APP = "android.dynamicmime.testapp";
    public static final String PACKAGE_ACTIVITIES = "android.dynamicmime.common.activity";
    public static final String PACKAGE_HELPER_APP = "android.dynamicmime.helper";
    public static final String PACKAGE_UPDATE_APP = "android.dynamicmime.update";
    public static final String PACKAGE_PREFERRED_APP = "android.dynamicmime.preferred";

    public static final String ACTIVITY_FIRST = ".FirstActivity";
    public static final String ACTIVITY_SECOND = ".SecondActivity";
    public static final String ACTIVITY_BOTH = ".TwoGroupsActivity";
    public static final String ACTIVITY_BOTH_AND_TYPE = ".TwoGroupsAndTypeActivity";

    public static final String DATA_DIR = "/data/local/tmp/dynamic-mime-test/";

    public static final String APK_PREFERRED_APP = DATA_DIR + "CtsDynamicMimePreferredApp.apk";
    public static final String APK_BOTH_GROUPS = DATA_DIR + "CtsDynamicMimeUpdateAppBothGroups.apk";
    public static final String APK_FIRST_GROUP = DATA_DIR + "CtsDynamicMimeUpdateAppFirstGroup.apk";
    public static final String APK_SECOND_GROUP = DATA_DIR +
            "CtsDynamicMimeUpdateAppSecondGroup.apk";

}
