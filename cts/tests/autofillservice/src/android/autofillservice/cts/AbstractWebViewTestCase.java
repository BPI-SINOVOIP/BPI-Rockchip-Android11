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

import org.junit.AfterClass;
import org.junit.BeforeClass;

public abstract class AbstractWebViewTestCase<A extends AbstractWebViewActivity>
        extends AutoFillServiceTestCase.AutoActivityLaunch<A> {

    protected AbstractWebViewTestCase() {
    }

    protected AbstractWebViewTestCase(UiBot inlineUiBot) {
        super(inlineUiBot);
    }

    // TODO(b/64951517): WebView currently does not trigger the autofill callbacks when values are
    // set using accessibility.
    protected static final boolean INJECT_EVENTS = true;

    @BeforeClass
    public static void setReplierMode() {
        sReplier.setIdMode(IdMode.HTML_NAME);
    }

    @AfterClass
    public static void resetReplierMode() {
        sReplier.setIdMode(IdMode.RESOURCE_ID);
    }
}
