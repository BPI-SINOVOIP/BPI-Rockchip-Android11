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

package android.autofillservice.cts.augmented;

import android.autofillservice.cts.AutofillActivityTestRule;
import android.autofillservice.cts.LoginNotImportantForAutofillWrappedActivityContextActivity;
import android.platform.test.annotations.AppModeFull;

@AppModeFull(reason = "AugmentedLoginActivityTest is enough")
public class AugmentedNotImportantForAutofillWrappedActivityContextTest extends
        AbstractLoginNotImportantForAutofillTestCase<
        LoginNotImportantForAutofillWrappedActivityContextActivity> {
    @Override
    protected AutofillActivityTestRule<LoginNotImportantForAutofillWrappedActivityContextActivity>
            getActivityRule() {
        return new AutofillActivityTestRule<
                LoginNotImportantForAutofillWrappedActivityContextActivity>(
                LoginNotImportantForAutofillWrappedActivityContextActivity.class) {
            @Override
            protected void afterActivityLaunched() {
                mActivity = getActivity();
            }
        };
    }
}
