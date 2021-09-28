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
package android.autofillservice.cts;

import android.content.Context;
import android.content.ContextWrapper;
import android.util.Log;

/**
 * Same as {@link LoginNotImportantForAutofillActivity}, but using a context wrapper of itself
 * as the base context.
 */
public class LoginNotImportantForAutofillWrappedActivityContextActivity
        extends LoginNotImportantForAutofillActivity {

    private Context mMyBaseContext;

    @Override
    public Context getBaseContext() {
        if (mMyBaseContext == null) {
            mMyBaseContext = new ContextWrapper(super.getBaseContext());
            Log.d(mTag, "getBaseContext(): set to " + mMyBaseContext + " (instead of "
                    + super.getBaseContext() + ")");
        }
        return mMyBaseContext;
    }
}
