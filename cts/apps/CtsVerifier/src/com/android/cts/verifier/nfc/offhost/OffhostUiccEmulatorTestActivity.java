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

package com.android.cts.verifier.nfc.offhost;

import com.android.cts.verifier.ArrayTestListAdapter;
import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;
import com.android.cts.verifier.TestListAdapter.TestListItem;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.nfc.NfcAdapter;
import android.nfc.cardemulation.CardEmulation;
import android.os.Bundle;

/** Activity that lists all the NFC Offhost-UICC emulator tests. */
public class OffhostUiccEmulatorTestActivity extends PassFailButtons.TestListActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.pass_fail_list);
        setInfoResources(R.string.nfc_test, R.string.nfc_offhost_uicc_emulator_test_info, 0);
        setPassFailButtonClickListeners();

        ArrayTestListAdapter adapter = new ArrayTestListAdapter(this);

        if (getPackageManager().hasSystemFeature(PackageManager.FEATURE_NFC_OFF_HOST_CARD_EMULATION_UICC)) {
            adapter.add(TestListItem.newCategory(this, R.string.nfc_offhost_uicc_emulator_tests));

            adapter.add(TestListItem.newTest(this, R.string.nfc_offhost_uicc_transaction_event1_emulator,
                    UiccTransactionEvent1EmulatorActivity.class.getName(),
                    new Intent(this, UiccTransactionEvent1EmulatorActivity.class), null));

            adapter.add(TestListItem.newTest(this, R.string.nfc_offhost_uicc_transaction_event2_emulator,
                    UiccTransactionEvent2EmulatorActivity.class.getName(),
                    new Intent(this, UiccTransactionEvent2EmulatorActivity.class), null));

            adapter.add(TestListItem.newTest(this, R.string.nfc_offhost_uicc_transaction_event3_emulator,
                    UiccTransactionEvent3EmulatorActivity.class.getName(),
                    new Intent(this, UiccTransactionEvent3EmulatorActivity.class), null));

        }

        setTestListAdapter(adapter);
    }
}
