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

package android.server.wm;

import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.LinkedList;
import java.util.function.Consumer;

/** A helper utility to track or manage object after a test method is done. */
public class ObjectTracker {
    private static final boolean DEBUG = "eng".equals(Build.TYPE);
    private LinkedList<AutoCloseable> mAutoCloseables;
    private LinkedList<ConsumableEntry> mConsumables;

    /** The interface used for tracking whether an object is consumed. */
    public interface Consumable {
        boolean isConsumed();
    }

    private static class ConsumableEntry {
        @NonNull
        final Consumable mConsumable;
        @Nullable
        final Throwable mStackTrace;

        ConsumableEntry(Consumable consumable, Throwable stackTrace) {
            mConsumable = consumable;
            mStackTrace = stackTrace;
        }
    }

    /**
     * If a {@link AutoCloseable} should be closed at the end of test, or it is not important when
     * to close, then we can use this method to manage the {@link AutoCloseable}. Then the extra
     * indents of try-with-resource can be eliminated. If the caller want to close the object
     * manually, it should use {@link ObjectTracker#close} to cancel the management.
     */
    public <T extends AutoCloseable> T manage(@NonNull T autoCloseable) {
        if (mAutoCloseables == null) {
            mAutoCloseables = new LinkedList<>();
        }
        mAutoCloseables.add(autoCloseable);
        return autoCloseable;
    }

    /**
     * Closes the {@link AutoCloseable} and remove from the managed list so it won't be closed twice
     * when leaving a test method.
     */
    public void close(@NonNull AutoCloseable autoCloseable) {
        if (mAutoCloseables == null) {
            return;
        }
        mAutoCloseables.remove(autoCloseable);
        try {
            autoCloseable.close();
        } catch (Throwable e) {
            throw new AssertionError("Failed to close " + autoCloseable, e);
        }
    }

    /** Tracks the {@link Consumable} to avoid misusing of the object that should do something. */
    public void track(@NonNull Consumable consumable) {
        if (mConsumables == null) {
            mConsumables = new LinkedList<>();
        }
        mConsumables.add(new ConsumableEntry(consumable,
                DEBUG ? new Throwable().fillInStackTrace() : null));
    }

    /**
     * Cleans up the managed object and make sure all tracked {@link Consumable} are consumed.
     * This method must be called after each test method.
     */
    public void tearDown(@NonNull Consumer<Throwable> errorConsumer) {
        ArrayList<Throwable> errors = null;
        if (mAutoCloseables != null) {
            while (!mAutoCloseables.isEmpty()) {
                final AutoCloseable autoCloseable = mAutoCloseables.removeLast();
                try {
                    autoCloseable.close();
                } catch (Throwable t) {
                    StateLogger.logE("Failed to close " + autoCloseable, t);
                    if (errors == null) {
                        errors = new ArrayList<>();
                    }
                    errors.add(t);
                }
            }
        }

        if (mConsumables != null) {
            while (!mConsumables.isEmpty()) {
                final ConsumableEntry entry = mConsumables.removeFirst();
                if (!entry.mConsumable.isConsumed()) {
                    StateLogger.logE("Found unconsumed object " + entry.mConsumable
                            + " that was created from:", entry.mStackTrace);
                    final Throwable t =
                            new IllegalStateException("Unconsumed object " + entry.mConsumable);
                    if (entry.mStackTrace != null) {
                        t.initCause(entry.mStackTrace);
                    }
                    if (errors == null) {
                        errors = new ArrayList<>();
                    }
                    errors.add(t);
                }
            }
        }

        if (errors != null) {
            errors.forEach(errorConsumer);
        }
    }
}
