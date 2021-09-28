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
import android.os.Parcel;
import android.text.Html;
import android.text.Spanned;
import android.text.SpannedString;

import com.google.android.car.diagnostictools.utils.DTCMetadata;
import com.google.android.car.diagnostictools.utils.MetadataProcessing;
import com.google.android.car.diagnostictools.utils.SelectableRowModel;

import java.util.Random;

/**
 * Model which wraps DTC code and metadata information for display through DTCAdapter. Extends
 * SelectableRowModel to allow for selection and use with SelectableAdapter
 */
public class DTC extends SelectableRowModel implements android.os.Parcelable {

    public static final Creator<DTC> CREATOR =
            new Creator<DTC>() {
                @Override
                public DTC createFromParcel(Parcel source) {
                    return new DTC(source);
                }

                @Override
                public DTC[] newArray(int size) {
                    return new DTC[size];
                }
            };
    private static final String TAG = "DTC";
    private static Random sRandomGen;
    private String mCode;
    private String mDescription;
    private long mTimestamp;
    private Spanned mLongDescription;
    private String mStringLongDescription; // Used for Parcelable compatibility

    /**
     * Full constructor for DTC.
     *
     * @param code DTC code associated with the DTC
     * @param description Short description associated with the DTC
     * @param timestamp Timestamp associated with the DTC
     * @param longDescription Long descriptions associated with the DTC
     * @param stringLongDescription String version of longDescription for Parcelable support
     */
    private DTC(
            String code,
            String description,
            long timestamp,
            Spanned longDescription,
            String stringLongDescription) {
        this.mCode = code;
        this.mDescription = description;
        this.mTimestamp = timestamp;
        this.mLongDescription = longDescription;
        this.mStringLongDescription = stringLongDescription;
    }

    /**
     * Paired down constructor that utilizes DTC metadata that is preloaded.
     *
     * @param code DTC code associated with the DTC
     * @param timestamp Timestamp associated with the DTC
     * @param context Context from which this is called. Required to allow MetadataProcessing to be
     *     loaded if the singleton has not been instantiated
     */
    DTC(String code, long timestamp, Context context) {
        this.mCode = code;
        this.mTimestamp = timestamp;
        DTCMetadata dtcMetadata = getDTCMetadata(code, context);
        if (dtcMetadata != null) {
            this.mDescription = dtcMetadata.getShortDescription();
            this.mLongDescription = dtcMetadata.getSpannedLongDescription();
            this.mStringLongDescription = dtcMetadata.getStringLongDescription();
        } else {
            this.mDescription = "No Description Available";
            this.mLongDescription = SpannedString.valueOf("No Details Available");
            this.mStringLongDescription = "No Details Available";
        }
    }

    protected DTC(Parcel in) {
        this.mCode = in.readString();
        this.mDescription = in.readString();
        this.mTimestamp = in.readLong();
        this.mStringLongDescription = in.readString();
        this.mLongDescription = Html.fromHtml(this.mStringLongDescription, 0);
    }

    public String getCode() {
        return mCode;
    }

    public String getDescription() {
        return mDescription;
    }

    public long getTimestamp() {
        return mTimestamp;
    }

    //Delete when DTCs are implemented
    public void setTimestamp(long timestamp) {
        mTimestamp = timestamp;
    }

    public Spanned getLongDescription() {
        return mLongDescription;
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
    static DTC createSampleDTC(int number, Context context) {
        String code = String.format("P%04d", 8 + number);
        if (sRandomGen == null) {
            sRandomGen = new Random();
        }
        long timestamp = sRandomGen.nextLong();
        return new DTC(code, timestamp, context);
    }

    private DTCMetadata getDTCMetadata(String code, Context context) {
        return MetadataProcessing.getInstance(context).getDTCMetadata(code);
    }

    /**
     * Implement method of SelectableRowModel. One element is selected as the DTC is the base child
     *
     * @return Returns 1 (as DTC is the based element)
     */
    @Override
    public int numSelected() {
        return 1;
    }

    /** TODO: Calls VHAL DTC delete method to allow this DTC to be cleared */
    @Override
    public void delete() {
        // TODO clear DTC
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(this.mCode);
        dest.writeString(this.mDescription);
        dest.writeLong(this.mTimestamp);
        dest.writeString(this.mStringLongDescription);
    }

    @Override
    public int describeContents() {
        return 0;
    }
}
