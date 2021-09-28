/*
 * Copyright (C) 2018 The Android Open Source Project
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
package android.autofillservice.cts;

import static androidx.test.platform.app.InstrumentationRegistry.getInstrumentation;

import android.service.autofill.CustomDescription;
import android.widget.RemoteViews;

public final class CustomDescriptionHelper {

    public static final String ID_SHOW = "show";
    public static final String ID_HIDE = "hide";
    public static final String ID_USERNAME_PLAIN = "username_plain";
    public static final String ID_USERNAME_MASKED = "username_masked";
    public static final String ID_PASSWORD_PLAIN = "password_plain";
    public static final String ID_PASSWORD_MASKED = "password_masked";

    private static final String sPackageName =
            getInstrumentation().getTargetContext().getPackageName();


    public static CustomDescription.Builder newCustomDescriptionWithUsernameAndPassword() {
        return new CustomDescription.Builder(new RemoteViews(sPackageName,
                R.layout.custom_description_with_username_and_password));
    }

    public static CustomDescription.Builder newCustomDescriptionWithHiddenFields() {
        return new CustomDescription.Builder(new RemoteViews(sPackageName,
                R.layout.custom_description_with_hidden_fields));
    }

    private CustomDescriptionHelper() {
        throw new UnsupportedOperationException("contain static methods only");
    }
}
