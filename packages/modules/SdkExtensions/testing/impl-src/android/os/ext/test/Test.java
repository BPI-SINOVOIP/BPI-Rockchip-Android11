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

package android.os.ext.test;

import android.annotation.SystemApi;
import android.annotation.TestApi;

/** This class exists purely to test new API additions are callable in tests. */
public class Test {

    public Test() {}

    public void publicMethod() {}

    /** @hide */
    @SystemApi
    public void systemApiMethod() {}

    /** @hide */
    @SystemApi(client = SystemApi.Client.MODULE_LIBRARIES)
    public void moduleLibsApiMethod() {}

    /** @hide */
    @TestApi
    public void testApiMethod() {}

    /** @hide */
    public void hiddenMethod() {}

}
