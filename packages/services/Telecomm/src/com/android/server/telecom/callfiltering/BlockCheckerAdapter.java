/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.server.telecom.callfiltering;

import android.content.Context;
import android.os.Bundle;
import android.provider.BlockedNumberContract;
import android.telecom.Log;

public class BlockCheckerAdapter {
    private static final String TAG = BlockCheckerAdapter.class.getSimpleName();

    public BlockCheckerAdapter() { }

    /**
     * Returns the call blocking status for the {@code phoneNumber}.
     * <p>
     * This method catches all underlying exceptions to ensure that this method never throws any
     * exception.
     *
     * @param context the context of the caller.
     * @param phoneNumber the number to check.
     * @param extras the extra attribute of the number.
     * @return result code indicating if the number should be blocked, and if so why.
     *         Valid values are: {@link BlockedNumberContract#STATUS_NOT_BLOCKED},
     *         {@link BlockedNumberContract#STATUS_BLOCKED_IN_LIST},
     *         {@link BlockedNumberContract#STATUS_BLOCKED_NOT_IN_CONTACTS},
     *         {@link BlockedNumberContract#STATUS_BLOCKED_PAYPHONE},
     *         {@link BlockedNumberContract#STATUS_BLOCKED_RESTRICTED},
     *         {@link BlockedNumberContract#STATUS_BLOCKED_UNKNOWN_NUMBER}.
     */
    public int getBlockStatus(Context context, String phoneNumber, Bundle extras) {
        int blockStatus = BlockedNumberContract.STATUS_NOT_BLOCKED;
        long startTimeNano = System.nanoTime();

        try {
            blockStatus = BlockedNumberContract.SystemContract.shouldSystemBlockNumber(
                    context, phoneNumber, extras);
            if (blockStatus != BlockedNumberContract.STATUS_NOT_BLOCKED) {
                Log.d(TAG, phoneNumber + " is blocked.");
            }
        } catch (Exception e) {
            Log.e(TAG, e, "Exception checking for blocked number");
        }

        int durationMillis = (int) ((System.nanoTime() - startTimeNano) / 1000000);
        if (durationMillis > 500 || Log.isLoggable(android.util.Log.DEBUG)) {
            Log.d(TAG, "Blocked number lookup took: " + durationMillis + " ms.");
        }
        return blockStatus;
    }
}
