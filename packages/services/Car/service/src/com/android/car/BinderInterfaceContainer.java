/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.car;

import android.annotation.Nullable;
import android.os.IBinder;
import android.os.IInterface;
import android.os.RemoteException;

import com.android.internal.annotations.GuardedBy;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;

/**
 * Helper class to hold client's binder interface.
 *
 * @param <T> type of the value that is wrapped by this class
 */
public class BinderInterfaceContainer<T extends IInterface> {

    /**
     * Wrapper class for objects that want to be notified whenever they are unliked from
     * the container ({@link BinderInterfaceContainer}).
     *
     * @param <T> type of the value that is wrapped by this class
     */
    public static class BinderInterface<T extends IInterface> implements IBinder.DeathRecipient {
        public final T binderInterface;
        private final BinderInterfaceContainer<T> mContainer;

        public BinderInterface(BinderInterfaceContainer<T> container, T binderInterface) {
            mContainer = container;
            this.binderInterface = binderInterface;
        }

        @Override
        public void binderDied() {
            binderInterface.asBinder().unlinkToDeath(this, 0);
            mContainer.handleBinderDeath(this);
        }
    }

    /**
     * Interface to be implemented by object that want to be notified whenever a binder is unliked
     * (dies).
     */
    public interface BinderEventHandler<T extends IInterface> {
        void onBinderDeath(BinderInterface<T> bInterface);
    }

    private final Object mLock = new Object();

    private final BinderEventHandler<T> mEventHandler;

    @GuardedBy("mLock")
    private final Map<IBinder, BinderInterface<T>> mBinders = new HashMap<>();

    /**
     * Constructs a new <code>BinderInterfaceContainer</code> passing an event handler to be used to
     * notify listeners when a registered binder dies (unlinked).
     */
    public BinderInterfaceContainer(@Nullable BinderEventHandler<T> eventHandler) {
        mEventHandler = eventHandler;
    }

    public BinderInterfaceContainer() {
        mEventHandler = null;
    }

    /**
     * Add the instance of {@link IInterface} representing the binder interface to this container.
     *
     * Internally, this object will be wrapped in an {@link BinderInterface} when added.
     */
    public void addBinder(T binderInterface) {
        IBinder binder = binderInterface.asBinder();
        synchronized (mLock) {
            BinderInterface<T> bInterface = mBinders.get(binder);
            if (bInterface != null) {
                return;
            }
            bInterface = new BinderInterface<T>(this, binderInterface);
            try {
                binder.linkToDeath(bInterface, 0);
            } catch (RemoteException e) {
                throw new IllegalArgumentException(e);
            }
            mBinders.put(binder, bInterface);
        }
    }

    /**
     * Removes the {@link BinderInterface} object associated with the passed parameter (if there is
     * any).
     */
    public void removeBinder(T binderInterface) {
        IBinder binder = binderInterface.asBinder();
        synchronized (mLock) {
            BinderInterface<T> bInterface = mBinders.get(binder);
            if (bInterface == null) {
                return;
            }
            binder.unlinkToDeath(bInterface, 0);
            mBinders.remove(binder);
        }
    }

    /**
     * Returns the {@link BinderInterface} object associated with the passed parameter.
     */
    public BinderInterface<T> getBinderInterface(T binderInterface) {
        IBinder binder = binderInterface.asBinder();
        synchronized (mLock) {
            return mBinders.get(binder);
        }
    }

    /**
     * Adds a new {@link BinderInterface} in this container.
     */
    public void addBinderInterface(BinderInterface<T> bInterface) {
        IBinder binder = bInterface.binderInterface.asBinder();
        synchronized (mLock) {
            try {
                binder.linkToDeath(bInterface, 0);
            } catch (RemoteException e) {
                throw new IllegalArgumentException(e);
            }
            mBinders.put(binder, bInterface);
        }
    }

    /**
     * Returns a shallow copy of all registered {@link BinderInterface} objects in this container.
     */
    public Collection<BinderInterface<T>> getInterfaces() {
        synchronized (mLock) {
            return new ArrayList<>(mBinders.values());
        }
    }

    /**
     * Returns the number of registered {@link BinderInterface} objects in this container.
     */
    public int size() {
        synchronized (mLock) {
            return mBinders.size();
        }
    }

    /**
     * Clears all registered {@link BinderInterface} objects.
     */
    public void clear() {
        synchronized (mLock) {
            Collection<BinderInterface<T>> interfaces = getInterfaces();
            for (BinderInterface<T> bInterface : interfaces) {
                IBinder binder = bInterface.binderInterface.asBinder();
                binder.unlinkToDeath(bInterface, 0);
            }
        }
        mBinders.clear();
    }

    private void handleBinderDeath(BinderInterface<T> bInterface) {
        if (mEventHandler != null) {
            mEventHandler.onBinderDeath(bInterface);
        }
        removeBinder(bInterface.binderInterface);
    }
}
