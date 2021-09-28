/*
 * Copyright 2017 The Android Open Source Project
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

import static android.autofillservice.cts.GridActivity.ID_L1C1;
import static android.autofillservice.cts.GridActivity.ID_L1C2;
import static android.autofillservice.cts.GridActivity.ID_L2C1;
import static android.autofillservice.cts.GridActivity.ID_L2C2;
import static android.autofillservice.cts.Helper.assertFillEventForContextCommitted;
import static android.autofillservice.cts.Helper.assertFillEventForFieldsClassification;
import static android.autofillservice.cts.Helper.findAutofillIdByResourceId;
import static android.provider.Settings.Secure.AUTOFILL_FEATURE_FIELD_CLASSIFICATION;
import static android.provider.Settings.Secure.AUTOFILL_USER_DATA_MAX_CATEGORY_COUNT;
import static android.provider.Settings.Secure.AUTOFILL_USER_DATA_MAX_FIELD_CLASSIFICATION_IDS_SIZE;
import static android.provider.Settings.Secure.AUTOFILL_USER_DATA_MAX_USER_DATA_SIZE;
import static android.provider.Settings.Secure.AUTOFILL_USER_DATA_MAX_VALUE_LENGTH;
import static android.provider.Settings.Secure.AUTOFILL_USER_DATA_MIN_VALUE_LENGTH;
import static android.service.autofill.AutofillFieldClassificationService.REQUIRED_ALGORITHM_CREDIT_CARD;
import static android.service.autofill.AutofillFieldClassificationService.REQUIRED_ALGORITHM_EDIT_DISTANCE;
import static android.service.autofill.AutofillFieldClassificationService.REQUIRED_ALGORITHM_EXACT_MATCH;

import static com.google.common.truth.Truth.assertThat;

import android.autofillservice.cts.Helper.FieldClassificationResult;
import android.os.Bundle;
import android.platform.test.annotations.AppModeFull;
import android.service.autofill.FillContext;
import android.service.autofill.FillEventHistory.Event;
import android.service.autofill.UserData;
import android.view.autofill.AutofillId;
import android.view.autofill.AutofillManager;
import android.widget.EditText;

import com.android.compatibility.common.util.SettingsStateChangerRule;

import org.junit.ClassRule;
import org.junit.Test;

import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

@AppModeFull(reason = "Service-specific test")
public class FieldsClassificationTest extends AbstractGridActivityTestCase {

    @ClassRule
    public static final SettingsStateChangerRule sFeatureEnabler =
            new SettingsStateChangerRule(sContext, AUTOFILL_FEATURE_FIELD_CLASSIFICATION, "1");

    @ClassRule
    public static final SettingsStateChangerRule sUserDataMaxFcSizeChanger =
            new SettingsStateChangerRule(sContext,
                    AUTOFILL_USER_DATA_MAX_FIELD_CLASSIFICATION_IDS_SIZE, "10");

    @ClassRule
    public static final SettingsStateChangerRule sUserDataMaxUserSizeChanger =
            new SettingsStateChangerRule(sContext, AUTOFILL_USER_DATA_MAX_USER_DATA_SIZE, "9");

    @ClassRule
    public static final SettingsStateChangerRule sUserDataMinValueChanger =
            new SettingsStateChangerRule(sContext, AUTOFILL_USER_DATA_MIN_VALUE_LENGTH, "4");

    @ClassRule
    public static final SettingsStateChangerRule sUserDataMaxValueChanger =
            new SettingsStateChangerRule(sContext, AUTOFILL_USER_DATA_MAX_VALUE_LENGTH, "50");

    @ClassRule
    public static final SettingsStateChangerRule sUserDataMaxCategoryChanger =
            new SettingsStateChangerRule(sContext, AUTOFILL_USER_DATA_MAX_CATEGORY_COUNT, "42");

    private AutofillManager mAfm;
    private final Bundle mLast4Bundle = new Bundle();
    private final Bundle mCreditCardBundle = new Bundle();

    @Override
    protected void postActivityLaunched() {
        mAfm = mActivity.getAutofillManager();
        mLast4Bundle.putInt("MATCH_SUFFIX", 4);

        mCreditCardBundle.putInt("REQUIRED_ARG_MIN_CC_LENGTH", 13);
        mCreditCardBundle.putInt("REQUIRED_ARG_MAX_CC_LENGTH", 19);
        mCreditCardBundle.putInt("OPTIONAL_ARG_SUFFIX_LENGTH", 4);
    }

    @Test
    public void testFeatureIsEnabled() throws Exception {
        enableService();
        assertThat(mAfm.isFieldClassificationEnabled()).isTrue();

        disableService();
        assertThat(mAfm.isFieldClassificationEnabled()).isFalse();
    }

    @Test
    public void testGetAlgorithm() throws Exception {
        enableService();

        // Check algorithms
        final List<String> names = mAfm.getAvailableFieldClassificationAlgorithms();
        assertThat(names.size()).isAtLeast(1);
        final String defaultAlgorithm = mAfm.getDefaultFieldClassificationAlgorithm();
        assertThat(defaultAlgorithm).isNotEmpty();
        assertThat(names).contains(defaultAlgorithm);

        // Checks invalid service
        disableService();
        assertThat(mAfm.getAvailableFieldClassificationAlgorithms()).isEmpty();
    }

    @Test
    public void testUserData() throws Exception {
        assertThat(mAfm.getUserData()).isNull();
        assertThat(mAfm.getUserDataId()).isNull();

        enableService();
        mAfm.setUserData(new UserData.Builder("user_data_id", "value", "remote_id")
                .build());
        assertThat(mAfm.getUserData()).isNotNull();
        assertThat(mAfm.getUserDataId()).isEqualTo("user_data_id");
        final UserData userData = mAfm.getUserData();
        assertThat(userData.getId()).isEqualTo("user_data_id");
        assertThat(userData.getFieldClassificationAlgorithm()).isNull();
        assertThat(userData.getFieldClassificationAlgorithms()).isNull();

        disableService();
        assertThat(mAfm.getUserData()).isNull();
        assertThat(mAfm.getUserDataId()).isNull();
    }

    @Test
    public void testRequiredAlgorithmsAvailable() throws Exception {
        enableService();
        final List<String> availableAlgorithms = mAfm.getAvailableFieldClassificationAlgorithms();
        assertThat(availableAlgorithms).isNotNull();
        assertThat(availableAlgorithms.contains(REQUIRED_ALGORITHM_EDIT_DISTANCE)).isTrue();
        assertThat(availableAlgorithms.contains(REQUIRED_ALGORITHM_EXACT_MATCH)).isTrue();
        assertThat(availableAlgorithms.contains(REQUIRED_ALGORITHM_CREDIT_CARD)).isTrue();
    }

    @Test
    public void testUserDataConstraints() throws Exception {
        // NOTE: values set by the SettingsStateChangerRule @Rules should have unique values to
        // make sure the getters below are reading the right property.
        assertThat(UserData.getMaxFieldClassificationIdsSize()).isEqualTo(10);
        assertThat(UserData.getMaxUserDataSize()).isEqualTo(9);
        assertThat(UserData.getMinValueLength()).isEqualTo(4);
        assertThat(UserData.getMaxValueLength()).isEqualTo(50);
        assertThat(UserData.getMaxCategoryCount()).isEqualTo(42);
    }

    @Test
    public void testHit_oneUserData_oneDetectableField() throws Exception {
        simpleHitTest(false, null);
    }

    @Test
    public void testHit_invalidAlgorithmIsIgnored() throws Exception {
        // For simplicity's sake, let's assume that name will never be valid..
        String invalidName = " ALGORITHM, Y NO INVALID? ";

        simpleHitTest(true, invalidName);
    }

    @Test
    public void testHit_userDataAlgorithmIsReset() throws Exception {
        simpleHitTest(true, null);
    }

    @Test
    public void testMiss_exactMatchAlgorithm() throws Exception {
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData
                .Builder("id", "t 1234", "cat")
                .setFieldClassificationAlgorithmForCategory("cat",
                        REQUIRED_ALGORITHM_EXACT_MATCH, mLast4Bundle)
                .build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field = mActivity.getCell(1, 1);
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1)
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Simulate user input
        mActivity.setText(1, 1, "t 5678");

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0), null);
    }

    @Test
    public void testHit_exactMatchLast4Algorithm() throws Exception {
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData
                .Builder("id", "1234", "cat")
                .setFieldClassificationAlgorithmForCategory("cat",
                        REQUIRED_ALGORITHM_EXACT_MATCH, mLast4Bundle)
                .build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1)
                .setVisitor((contexts, builder) -> fieldId
                        .set(findAutofillIdByResourceId(contexts.get(0), ID_L1C1)))
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Simulate user input
        mActivity.setText(1, 1, "T1234");

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0), fieldId.get(), "cat", 1);
    }

    @Test
    public void testHit_CreditCardAlgorithm() throws Exception {
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData
                .Builder("id", "1122334455667788", "card")
                .setFieldClassificationAlgorithmForCategory("card",
                        REQUIRED_ALGORITHM_CREDIT_CARD, mCreditCardBundle)
                .build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1)
                .setVisitor((contexts, builder) -> fieldId
                        .set(findAutofillIdByResourceId(contexts.get(0), ID_L1C1)))
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Simulate user input
        mActivity.setText(1, 1, "7788");

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0), fieldId.get(), "card", 1);
    }

    @Test
    public void testHit_useDefaultAlgorithm() throws Exception {
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData
                .Builder("id", "1234", "cat")
                .setFieldClassificationAlgorithm(REQUIRED_ALGORITHM_EXACT_MATCH, mLast4Bundle)
                .setFieldClassificationAlgorithmForCategory("dog",
                        REQUIRED_ALGORITHM_EDIT_DISTANCE, null)
                .build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1)
                .setVisitor((contexts, builder) -> fieldId
                        .set(findAutofillIdByResourceId(contexts.get(0), ID_L1C1)))
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Simulate user input
        mActivity.setText(1, 1, "T1234");

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0), fieldId.get(), "cat", 1);
    }

    private void simpleHitTest(boolean setAlgorithm, String algorithm) throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        final UserData.Builder userData = new UserData.Builder("id", "FULLY", "myId");
        if (setAlgorithm) {
            userData.setFieldClassificationAlgorithm(algorithm, null);
        }
        mAfm.setUserData(userData.build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1)
                .setVisitor((contexts, builder) -> fieldId
                        .set(findAutofillIdByResourceId(contexts.get(0), ID_L1C1)))
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Simulate user input
        mActivity.setText(1, 1, "fully");

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0), fieldId.get(), "myId", 1);
    }

    @Test
    public void testHit_sameValueForMultipleCategories() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData
                .Builder("id", "FULLY", "cat1")
                .add("FULLY", "cat2")
                .build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1)
                .setVisitor((contexts, builder) -> fieldId
                        .set(findAutofillIdByResourceId(contexts.get(0), ID_L1C1)))
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Simulate user input
        mActivity.setText(1, 1, "fully");

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0),
                new FieldClassificationResult[] {
                        new FieldClassificationResult(fieldId.get(),
                                new String[] { "cat1", "cat2"},
                                new float[] {1, 1})
                });
    }

    @Test
    public void testHit_manyUserData_oneDetectableField_bestMatchIsFirst() throws Exception {
        manyUserData_oneDetectableField(true);
    }

    @Test
    public void testHit_manyUserData_oneDetectableField_bestMatchIsSecond() throws Exception {
        manyUserData_oneDetectableField(false);
    }

    private void manyUserData_oneDetectableField(boolean firstMatch) throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData.Builder("id", "Iam1ST", "1stId")
                .add("Iam2ND", "2ndId").build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1)
                .setVisitor((contexts, builder) -> fieldId
                        .set(findAutofillIdByResourceId(contexts.get(0), ID_L1C1)))
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Simulate user input
        mActivity.setText(1, 1, firstMatch ? "IAM111" : "IAM222");

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        // Best match is 0.66 (4 of 6), worst is 0.5 (3 of 6)
        if (firstMatch) {
            assertFillEventForFieldsClassification(events.get(0), new FieldClassificationResult[] {
                    new FieldClassificationResult(fieldId.get(), new String[] { "1stId", "2ndId" },
                            new float[] { 0.66F, 0.5F })});
        } else {
            assertFillEventForFieldsClassification(events.get(0), new FieldClassificationResult[] {
                    new FieldClassificationResult(fieldId.get(), new String[] { "2ndId", "1stId" },
                            new float[] { 0.66F, 0.5F }) });
        }
    }

    @Test
    public void testHit_oneUserData_manyDetectableFields() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData.Builder("id", "FULLY", "myId").build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field1 = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId1 = new AtomicReference<>();
        final AtomicReference<AutofillId> fieldId2 = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1, ID_L1C2)
                .setVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    fieldId1.set(findAutofillIdByResourceId(context, ID_L1C1));
                    fieldId2.set(findAutofillIdByResourceId(context, ID_L1C2));
                })
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field1);

        // Simulate user input
        mActivity.setText(1, 1, "fully"); // 100%
        mActivity.setText(1, 2, "fooly"); // 60%

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0),
                new FieldClassificationResult[] {
                        new FieldClassificationResult(fieldId1.get(), "myId", 1.0F),
                        new FieldClassificationResult(fieldId2.get(), "myId", 0.6F),
                });
    }

    @Test
    public void testHit_manyUserData_manyDetectableFields() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData.Builder("id", "FULLY", "myId")
                .add("ZZZZZZZZZZ", "totalMiss") // should not have matched any
                .add("EMPTY", "otherId")
                .build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field1 = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId1 = new AtomicReference<>();
        final AtomicReference<AutofillId> fieldId2 = new AtomicReference<>();
        final AtomicReference<AutofillId> fieldId3 = new AtomicReference<>();
        final AtomicReference<AutofillId> fieldId4 = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1, ID_L1C2)
                .setVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    fieldId1.set(findAutofillIdByResourceId(context, ID_L1C1));
                    fieldId2.set(findAutofillIdByResourceId(context, ID_L1C2));
                    fieldId3.set(findAutofillIdByResourceId(context, ID_L2C1));
                    fieldId4.set(findAutofillIdByResourceId(context, ID_L2C2));
                })
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field1);

        // Simulate user input
        mActivity.setText(1, 1, "fully"); // u1: 100% u2:  20%
        mActivity.setText(1, 2, "empty"); // u1:  20% u2: 100%
        mActivity.setText(2, 1, "fooly"); // u1:  60% u2:  20%
        mActivity.setText(2, 2, "emppy"); // u1:  20% u2:  80%

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0),
                new FieldClassificationResult[] {
                        new FieldClassificationResult(fieldId1.get(),
                                new String[] { "myId", "otherId" }, new float[] { 1.0F, 0.2F }),
                        new FieldClassificationResult(fieldId2.get(),
                                new String[] { "otherId", "myId" }, new float[] { 1.0F, 0.2F }),
                        new FieldClassificationResult(fieldId3.get(),
                                new String[] { "myId", "otherId" }, new float[] { 0.6F, 0.2F }),
                        new FieldClassificationResult(fieldId4.get(),
                                new String[] { "otherId", "myId"}, new float[] { 0.80F, 0.2F })});
    }

    @Test
    public void testHit_manyUserData_manyDetectableFields_differentClassificationAlgo()
            throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData.Builder("id", "1234", "myId")
                .add("ZZZZZZZZZZ", "totalMiss") // should not have matched any
                .add("EMPTY", "otherId")
                .setFieldClassificationAlgorithmForCategory("myId",
                        REQUIRED_ALGORITHM_EXACT_MATCH, mLast4Bundle)
                .setFieldClassificationAlgorithmForCategory("otherId",
                        REQUIRED_ALGORITHM_EDIT_DISTANCE, null)
                .build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field1 = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId1 = new AtomicReference<>();
        final AtomicReference<AutofillId> fieldId2 = new AtomicReference<>();
        final AtomicReference<AutofillId> fieldId3 = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1, ID_L1C2)
                .setVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    fieldId1.set(findAutofillIdByResourceId(context, ID_L1C1));
                    fieldId2.set(findAutofillIdByResourceId(context, ID_L1C2));
                    fieldId3.set(findAutofillIdByResourceId(context, ID_L2C1));
                })
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field1);

        // Simulate user input
        mActivity.setText(1, 1, "E1234"); // u1: 100% u2:  20%
        mActivity.setText(1, 2, "empty"); // u1:   0% u2: 100%
        mActivity.setText(2, 1, "fULLy"); // u1:   0% u2:  20%

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0),
                new FieldClassificationResult[] {
                        new FieldClassificationResult(fieldId1.get(),
                                new String[] { "myId", "otherId" }, new float[] { 1.0F, 0.2F }),
                        new FieldClassificationResult(fieldId2.get(),
                                new String[] { "otherId" }, new float[] { 1.0F }),
                        new FieldClassificationResult(fieldId3.get(),
                                new String[] { "otherId" }, new float[] { 0.2F })});
    }

    @Test
    public void testHit_manyUserDataPerField_manyDetectableFields() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData.Builder("id", "zzzzz", "myId") // should not have matched any
                .add("FULL1", "myId") // match 80%, should not have been reported
                .add("FULLY", "myId") // match 100%
                .add("ZZZZZZZZZZ", "totalMiss") // should not have matched any
                .add("EMPTY", "otherId")
                .build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field1 = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId1 = new AtomicReference<>();
        final AtomicReference<AutofillId> fieldId2 = new AtomicReference<>();
        final AtomicReference<AutofillId> fieldId3 = new AtomicReference<>();
        final AtomicReference<AutofillId> fieldId4 = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1, ID_L1C2)
                .setVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    fieldId1.set(findAutofillIdByResourceId(context, ID_L1C1));
                    fieldId2.set(findAutofillIdByResourceId(context, ID_L1C2));
                    fieldId3.set(findAutofillIdByResourceId(context, ID_L2C1));
                    fieldId4.set(findAutofillIdByResourceId(context, ID_L2C2));
                })
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field1);

        // Simulate user input
        mActivity.setText(1, 1, "fully"); // u1: 100% u2:  20%
        mActivity.setText(1, 2, "empty"); // u1:  20% u2: 100%
        mActivity.setText(2, 1, "fooly"); // u1:  60% u2:  20%
        mActivity.setText(2, 2, "emppy"); // u1:  20% u2:  80%

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0),
                new FieldClassificationResult[] {
                        new FieldClassificationResult(fieldId1.get(),
                                new String[] { "myId", "otherId" }, new float[] { 1.0F, 0.2F }),
                        new FieldClassificationResult(fieldId2.get(),
                                new String[] { "otherId", "myId" }, new float[] { 1.0F, 0.2F }),
                        new FieldClassificationResult(fieldId3.get(),
                                new String[] { "myId", "otherId" }, new float[] { 0.6F, 0.2F }),
                        new FieldClassificationResult(fieldId4.get(),
                                new String[] { "otherId", "myId"}, new float[] { 0.80F, 0.2F })});
    }

    @Test
    public void testMiss() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData.Builder("id", "ABCDEF", "myId").build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field = mActivity.getCell(1, 1);
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1)
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Simulate user input
        mActivity.setText(1, 1, "xyz");

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForContextCommitted(events.get(0));
    }

    @Test
    public void testNoUserInput() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData.Builder("id", "FULLY", "myId").build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field = mActivity.getCell(1, 1);
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1)
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForContextCommitted(events.get(0));
    }

    @Test
    public void testHit_usePackageUserData() throws Exception {
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData
                .Builder("id", "TEST1", "cat")
                .setFieldClassificationAlgorithm(null, null)
                .build());

        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId1 = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1)
                .setVisitor((contexts, builder) -> fieldId1
                        .set(findAutofillIdByResourceId(contexts.get(0), ID_L1C1)))
                .setUserData(new UserData.Builder("id2", "TEST2", "cat")
                        .setFieldClassificationAlgorithm(null, null)
                        .build())
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Simulate user input
        mActivity.setText(1, 1, "test1");

        // Finish context
        mAfm.commit();

        final Event packageUserDataEvent = InstrumentedAutoFillService.getFillEvents(1).get(0);
        assertFillEventForFieldsClassification(packageUserDataEvent, fieldId1.get(), "cat", 0.8F);

        final AtomicReference<AutofillId> fieldId2 = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setVisitor((contexts, builder) -> fieldId2
                        .set(findAutofillIdByResourceId(contexts.get(0), ID_L1C1)))
                .setFieldClassificationIds(ID_L1C1)
                .build());

        // Need to switch focus first
        mActivity.focusCell(1, 2);

        // Trigger second autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field);

        // Finish context.
        mAfm.commit();

        // Assert results
        final Event defaultUserDataEvent = InstrumentedAutoFillService.getFillEvents(1).get(0);
        assertFillEventForFieldsClassification(defaultUserDataEvent, fieldId2.get(), "cat", 1.0F);
    }

    @Test
    public void testHit_mergeUserData_manyDetectableFields() throws Exception {
        // Set service.
        enableService();

        // Set expectations.
        mAfm.setUserData(new UserData.Builder("id", "FULLY", "myId").build());
        final MyAutofillCallback callback = mActivity.registerCallback();
        final EditText field1 = mActivity.getCell(1, 1);
        final AtomicReference<AutofillId> fieldId1 = new AtomicReference<>();
        final AtomicReference<AutofillId> fieldId2 = new AtomicReference<>();
        sReplier.addResponse(new CannedFillResponse.Builder()
                .setFieldClassificationIds(ID_L1C1, ID_L1C2)
                .setVisitor((contexts, builder) -> {
                    final FillContext context = contexts.get(0);
                    fieldId1.set(findAutofillIdByResourceId(context, ID_L1C1));
                    fieldId2.set(findAutofillIdByResourceId(context, ID_L1C2));
                })
                .setUserData(new UserData.Builder("id2", "FOOLY", "otherId")
                        .add("EMPTY", "myId")
                        .build())
                .build());

        // Trigger autofill
        mActivity.focusCell(1, 1);
        sReplier.getNextFillRequest();

        mUiBot.assertNoDatasetsEver();
        callback.assertUiUnavailableEvent(field1);

        // Simulate user input
        mActivity.setText(1, 1, "fully"); // u1:  20%, u2: 60%
        mActivity.setText(1, 2, "empty"); // u1: 100%, u2: 20%

        // Finish context.
        mAfm.commit();

        // Assert results
        final List<Event> events = InstrumentedAutoFillService.getFillEvents(1);
        assertFillEventForFieldsClassification(events.get(0),
                new FieldClassificationResult[] {
                        new FieldClassificationResult(fieldId1.get(),
                                new String[] { "otherId", "myId" }, new float[] { 0.6F, 0.2F }),
                        new FieldClassificationResult(fieldId2.get(),
                                new String[] { "myId", "otherId" }, new float[] { 1.0F, 0.2F }),
                });
    }

    /*
     * TODO(b/73648631): other scenarios:
     *
     * - Multipartition (for example, one response with FieldsDetection, others with datasets,
     *   saveinfo, and/or ignoredIds)
     * - make sure detectable fields don't trigger a new partition
     * v test partial hit (for example, 'fool' instead of 'full'
     * v multiple fields
     * v multiple value
     * - combinations of above items
     */
}
