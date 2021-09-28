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
package com.android.tv.common.support.tvprovider;

import android.net.Uri;
import android.support.annotation.Nullable;
import androidx.tvprovider.media.tv.TvContractCompat;

/**
 * Extensions to the contract between the TV provider and applications. Contains definitions for the
 * supported URIs and columns.
 *
 * <p>TODO(b/126921088): move this to androidx.
 */
public final class TvContractCompatX {

    /**
     * Builds a URI that points to a specific channel.
     *
     * @param inputPackage the package of the input.
     * @param internalProviderId the internal provider id
     */
    public static Uri buildChannelUri(
            @Nullable String inputPackage, @Nullable String internalProviderId) {
        Uri.Builder uri = TvContractCompat.Channels.CONTENT_URI.buildUpon();
        if (inputPackage != null) {
            uri.appendQueryParameter("package", inputPackage);
        }
        if (internalProviderId != null) {
            uri.appendQueryParameter(
                    TvContractCompat.Channels.COLUMN_INTERNAL_PROVIDER_ID, internalProviderId);
        }
        return uri.build();
    }

    /**
     * Builds a URI that points to all programs on a given channel.
     *
     * @param inputPackage the package of the input.
     * @param internalProviderId the internal provider id
     */
    public static Uri buildProgramsUriForChannel(
            @Nullable String inputPackage, @Nullable String internalProviderId) {
        Uri.Builder uri = TvContractCompat.Programs.CONTENT_URI.buildUpon();
        if (inputPackage != null) {
            uri.appendQueryParameter("package", inputPackage);
        }
        if (internalProviderId != null) {
            uri.appendQueryParameter(
                    TvContractCompat.Channels.COLUMN_INTERNAL_PROVIDER_ID, internalProviderId);
        }
        return uri.build();
    }

    /**
     * Builds a URI that points to programs on a specific channel whose schedules overlap with the
     * given time frame.
     *
     * @param inputPackage the package of the input.
     * @param internalProviderId the internal provider id
     * @param startTime The start time used to filter programs. The returned programs should have
     *     {@link TvContractCompat.Programs#COLUMN_END_TIME_UTC_MILLIS} that is greater than this
     *     time.
     * @param endTime The end time used to filter programs. The returned programs should have {@link
     *     TvContractCompat.Programs#COLUMN_START_TIME_UTC_MILLIS} that is less than this time.
     */
    public static Uri buildProgramsUriForChannel(
            @Nullable String inputPackage,
            @Nullable String internalProviderId,
            long startTime,
            long endTime) {
        return buildProgramsUriForChannel(inputPackage, internalProviderId)
                .buildUpon()
                .appendQueryParameter(TvContractCompat.PARAM_START_TIME, String.valueOf(startTime))
                .appendQueryParameter(TvContractCompat.PARAM_END_TIME, String.valueOf(endTime))
                .build();
    }

    /**
     * Builds a URI that points to programs whose schedules overlap with the given time frame.
     *
     * @param startTime The start time used to filter programs. The returned programs should have
     *     {@link TvContractCompat.Programs#COLUMN_END_TIME_UTC_MILLIS} that is greater than this
     *     time.
     * @param endTime The end time used to filter programs. The returned programs should have {@link
     *     TvContractCompat.Programs#COLUMN_START_TIME_UTC_MILLIS} that is less than this time.
     */
    public static Uri buildProgramsUri(long startTime, long endTime) {
        return TvContractCompat.Programs.CONTENT_URI
                .buildUpon()
                .appendQueryParameter(TvContractCompat.PARAM_START_TIME, String.valueOf(startTime))
                .appendQueryParameter(TvContractCompat.PARAM_END_TIME, String.valueOf(endTime))
                .build();
    }
}
