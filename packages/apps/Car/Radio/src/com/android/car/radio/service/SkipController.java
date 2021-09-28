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
package com.android.car.radio.service;

import android.os.RemoteException;

import androidx.annotation.GuardedBy;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.LiveData;

import com.android.car.broadcastradio.support.Program;
import com.android.car.radio.SkipMode;
import com.android.car.radio.util.Log;

import java.io.PrintWriter;
import java.util.List;

/**
 * Helper class used to keep track of which station should be toggled next (or prev), based on
 * {@link SkipMode}.
 */
final class SkipController {

    private static final String TAG = SkipController.class.getSimpleName();

    private final Object mLock = new Object();

    private final IRadioAppService.Stub mService;

    @GuardedBy("mlock")
    private List<Program> mFavorites;

    @GuardedBy("mlock")
    private int mCurrentIndex;

    @GuardedBy("mlock")
    private SkipMode mSkipMode;

    SkipController(@NonNull IRadioAppService.Stub service,
            @NonNull LiveData<List<Program>> favorites, @NonNull SkipMode initialMode) {
        mService = service;
        mSkipMode = initialMode;

        Log.v(TAG, "Initial mode: %s", initialMode);

        // TODO(b/137647889): not really working because they're changed in a different process.
        // As such, the changes are only effective after the radio service restarts - that's
        // not ideal, but it's better than nothing :-)
        // Long term, we need to provide a way to sync them...
        favorites.observeForever(this::onFavoritesChanged);
    }

    void setSkipMode(@NonNull SkipMode mode) {
        Log.d(TAG, "setSkipMode(%s)", mode);
        synchronized (mLock) {
            this.mSkipMode = mode;
        }
    }

    void skip(boolean forward, ITuneCallback callback) throws RemoteException {
        Log.d(TAG, "skip(%s, %s)", mSkipMode, forward);

        Program program = null;
        synchronized (mLock) {
            if (mSkipMode == SkipMode.FAVORITES || mSkipMode == SkipMode.BROWSE) {
                program = getFavoriteLocked(forward);
                if (program == null) {
                    Log.d(TAG, "skip(%s): no favorites, seeking instead", forward);
                }
            }
        }

        if (program != null) {
            Log.d(TAG, "skip(%s): changing to %s", forward, program.getName());
            mService.tune(program.getSelector(), callback);
        } else {
            mService.seek(forward, callback);
        }
    }

    private void onFavoritesChanged(List<Program> favorites) {
        Log.v(TAG, "onFavoritesChanged(): %s", favorites);
        synchronized (this) {
            mFavorites = favorites;
            // TODO(b/137647889): try to preserve currentIndex, either pointing to the same station,
            // or the closest one
            mCurrentIndex = 0;
        }
    }

    @Nullable
    private Program getFavoriteLocked(boolean next) {
        if (mFavorites == null || mFavorites.isEmpty()) return null;

        // TODO(b/137647889): to keep it simple, we're only interacting through explicit calls
        // to prev/next, but ideally it should also take in account the current station.
        // For example, say the favorites are 4, 8, 15, 16, 23, 42 and user skipped from
        // 15 to 16 but later manually tuned to 5. In this case, if the user skips again we'll
        // return 23 (next index), but ideally it would be 8 (i.e, next favorite whose value
        // is higher than 5)
        if (next) {
            mCurrentIndex++;
            if (mCurrentIndex >= mFavorites.size()) {
                mCurrentIndex = 0;
            }
        } else {
            mCurrentIndex--;
            if (mCurrentIndex < 0) {
                mCurrentIndex = mFavorites.size() - 1;
            }
        }
        Program program = mFavorites.get(mCurrentIndex);
        Log.v(TAG, "getting favorite #" + mCurrentIndex + ": " + program.getName());
        return program;
    }

    void dump(@NonNull PrintWriter pw, @NonNull String prefix) {
        synchronized (mLock) {
            pw.print(prefix); pw.print("mode: "); pw.println(mSkipMode);
            pw.print(prefix); pw.print("current index: "); pw.println(mCurrentIndex);
            if (mFavorites == null || mFavorites.isEmpty()) {
                pw.print(prefix); pw.println("no favorites");
                return;
            }
            int size = mFavorites.size();
            pw.print(prefix); pw.print(size); pw.println(" favorites: ");
            String prefix2 = prefix + "  ";
            for (int i = 0; i < size; i++) {
                pw.print(prefix2);
                pw.print(i); pw.print(": "); pw.print(mFavorites.get(i).getName());
                if (i == mCurrentIndex) {
                    pw.print(" (current)");
                }
                pw.println();
            }
        }
    }
}
