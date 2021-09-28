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

package android.autofillservice.cts.inline;

import android.autofillservice.cts.InstrumentedAutoFillService;
import android.service.autofill.AutofillService;

/**
 * Implementation of {@link AutofillService} that has inline suggestions support enabled.
 */
public class InstrumentedAutoFillServiceInlineEnabled extends InstrumentedAutoFillService {
    @SuppressWarnings("hiding")
    static final String SERVICE_PACKAGE = "android.autofillservice.cts";
    @SuppressWarnings("hiding")
    static final String SERVICE_CLASS = "InstrumentedAutoFillServiceInlineEnabled";
    @SuppressWarnings("hiding")
    static final String SERVICE_NAME = SERVICE_PACKAGE + "/.inline." + SERVICE_CLASS;

    public InstrumentedAutoFillServiceInlineEnabled() {
        sInstance.set(this);
        sServiceLabel = SERVICE_CLASS;
    }
}
