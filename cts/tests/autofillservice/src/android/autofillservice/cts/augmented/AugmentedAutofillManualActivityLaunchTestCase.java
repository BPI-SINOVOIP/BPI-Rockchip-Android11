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

import static android.autofillservice.cts.Helper.allowOverlays;
import static android.autofillservice.cts.Helper.disallowOverlays;

import android.autofillservice.cts.AutoFillServiceTestCase;
import android.autofillservice.cts.augmented.CtsAugmentedAutofillService.AugmentedReplier;
import android.content.AutofillOptions;
import android.view.autofill.AutofillManager;

import com.android.compatibility.common.util.RequiredSystemResourceRule;

import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;

/////
///// NOTE: changes in this class should also be applied to
/////       AugmentedAutofillManualActivityLaunchTestCase, which is exactly the same as this except
/////       by which class it extends.

// Must be public because of the @ClassRule
public abstract class AugmentedAutofillManualActivityLaunchTestCase
        extends AutoFillServiceTestCase.ManualActivityLaunch {

    protected static AugmentedReplier sAugmentedReplier;
    protected AugmentedUiBot mAugmentedUiBot;

    private CtsAugmentedAutofillService.ServiceWatcher mServiceWatcher;

    private static final RequiredSystemResourceRule sRequiredResource =
            new RequiredSystemResourceRule("config_defaultAugmentedAutofillService");

    private static final RuleChain sRequiredFeatures = RuleChain
            .outerRule(sRequiredFeatureRule)
            .around(sRequiredResource);

    @BeforeClass
    public static void allowAugmentedAutofill() {
        sContext.getApplicationContext()
                .setAutofillOptions(AutofillOptions.forWhitelistingItself());
        allowOverlays();
    }

    @AfterClass
    public static void resetAllowAugmentedAutofill() {
        sContext.getApplicationContext().setAutofillOptions(null);
        disallowOverlays();
    }

    @Before
    public void setFixtures() {
        sAugmentedReplier = CtsAugmentedAutofillService.getAugmentedReplier();
        sAugmentedReplier.reset();
        CtsAugmentedAutofillService.resetStaticState();
        mAugmentedUiBot = new AugmentedUiBot(mUiBot);
        mSafeCleanerRule
                .run(() -> sAugmentedReplier.assertNoUnhandledFillRequests())
                .run(() -> {
                    AugmentedHelper.resetAugmentedService();
                    if (mServiceWatcher != null) {
                        mServiceWatcher.waitOnDisconnected();
                    }
                })
                .add(() -> {
                    return sAugmentedReplier.getExceptions();
                });
    }

    @Override
    protected int getSmartSuggestionMode() {
        return AutofillManager.FLAG_SMART_SUGGESTION_SYSTEM;
    }

    @Override
    protected TestRule getRequiredFeaturesRule() {
        return sRequiredFeatures;
    }

    protected CtsAugmentedAutofillService enableAugmentedService() throws InterruptedException {
        return enableAugmentedService(/* whitelistSelf= */ true);
    }

    protected CtsAugmentedAutofillService enableAugmentedService(boolean whitelistSelf)
            throws InterruptedException {
        if (mServiceWatcher != null) {
            throw new IllegalStateException("There Can Be Only One!");
        }

        mServiceWatcher = CtsAugmentedAutofillService.setServiceWatcher();
        AugmentedHelper.setAugmentedService(CtsAugmentedAutofillService.SERVICE_NAME);

        CtsAugmentedAutofillService service = mServiceWatcher.waitOnConnected();
        service.waitUntilConnected();
        return service;
    }
}
