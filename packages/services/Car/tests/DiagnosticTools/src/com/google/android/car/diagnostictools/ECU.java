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

package com.google.android.car.diagnostictools;

import android.content.Context;

import com.google.android.car.diagnostictools.utils.MetadataProcessing;
import com.google.android.car.diagnostictools.utils.SelectableRowModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Model which wraps ECU data (address, name, and associated DTCs) and extends SelectableRowModel to
 * enable the selection of ECU elements.
 */
public class ECU extends SelectableRowModel {

    private String mAddress;
    private String mName;
    /**
     * MUST be locked when integrating DTC properties to prevent issues with parallel adding and
     * deleting.
     */
    private List<DTC> mDtcs;

    /**
     * Full constructor that creates an ECU model with its address, name, and list of associated
     * DTCs.
     *
     * @param address Address for ECU
     * @param name Human readable name for ECU
     * @param dtcs List of DTCs that are raised by the ECU
     */
    private ECU(String address, String name, List<DTC> dtcs) {
        mAddress = address;
        mName = name;
        this.mDtcs = dtcs;
    }

    /**
     * Paired down constructor that creates an ECU based on its address, list of associated DTCs,
     * and metadata.
     *
     * @param address Address for ECU
     * @param dtcs List of DTCs that are raised by the ECU
     * @param context Context from which this is called. Required to allow MetadataProcessing to be
     *     loaded if the singleton has not been instantiated
     */
    public ECU(String address, List<DTC> dtcs, Context context) {
        mAddress = address;
        this.mDtcs = dtcs;
        String metadata = MetadataProcessing.getInstance(context).getECUMetadata(address);
        if (metadata != null) {
            this.mName = metadata;
        } else {
            this.mName = "No Name Available";
        }
    }

    public String getAddress() {
        return mAddress;
    }

    public String getName() {
        return mName;
    }

    public List<DTC> getDtcs() {
        return mDtcs;
    }

    /**
     * Create sample DTC using MetadataProcessing for values. Some numbers will not return actual
     * values and will be replaced with placeholder information that would be presented if metadata
     * wasn't found.
     *
     * @param number Used to generate the DTC code which will be "P(008+number)"
     * @param context Context from which this is called. Required to allow MetadataProcessing to be
     *     loaded if the singleton has not been instantiated
     * @return New sample DTC based on number and metadata
     */
    static ECU createSampleECU(int number, Context context) {
        List<DTC> dtcs = new ArrayList<>();
        String address = (number + 123) + "";

        ECU rtrECU = new ECU(address, dtcs, context);

        for (int i = 0; i < number; i++) {
            dtcs.add(DTC.createSampleDTC(i, context));
        }
        return rtrECU;
    }

    /**
     * Implement method of SelectableRowModel. The number of elements selected is based on the
     * number of elements in its children
     *
     * @return Returns the number of DTCs that are its children (as DTC is the based element)
     */
    @Override
    public int numSelected() {
        int count = 0;
        for (DTC dtc : mDtcs) {
            count += dtc.numSelected();
        }
        return count;
    }

    /** Runs the implemented delete method in the DTC model to delete records on a VHAL level */
    @Override
    public void delete() {
        for (DTC dtc : mDtcs) {
            dtc.delete();
        }
    }
}
