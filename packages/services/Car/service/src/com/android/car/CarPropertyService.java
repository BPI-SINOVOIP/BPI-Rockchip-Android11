/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static java.lang.Integer.toHexString;

import android.car.Car;
import android.car.hardware.CarPropertyConfig;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyEvent;
import android.car.hardware.property.ICarProperty;
import android.car.hardware.property.ICarPropertyEventListener;
import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.util.Pair;
import android.util.SparseArray;

import com.android.car.hal.PropertyHalService;
import com.android.internal.annotations.GuardedBy;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * This class implements the binder interface for ICarProperty.aidl to make it easier to create
 * multiple managers that deal with Vehicle Properties. The property Ids in this class are IDs in
 * manager level.
 */
public class CarPropertyService extends ICarProperty.Stub
        implements CarServiceBase, PropertyHalService.PropertyHalListener {
    private static final boolean DBG = true;
    private static final String TAG = "Property.service";
    private final Context mContext;
    private final Map<IBinder, Client> mClientMap = new ConcurrentHashMap<>();
    @GuardedBy("mLock")
    private final Map<Integer, CarPropertyConfig<?>> mConfigs = new HashMap<>();
    private final PropertyHalService mHal;
    private boolean mListenerIsSet = false;
    private final Map<Integer, List<Client>> mPropIdClientMap = new ConcurrentHashMap<>();
    private final Object mLock = new Object();
    @GuardedBy("mLock")
    private final SparseArray<SparseArray<Client>> mSetOperationClientMap = new SparseArray<>();
    private final HandlerThread mHandlerThread =
            CarServiceUtils.getHandlerThread(getClass().getSimpleName());
    private final Handler mHandler = new Handler(mHandlerThread.getLooper());

    public CarPropertyService(Context context, PropertyHalService hal) {
        if (DBG) {
            Log.d(TAG, "CarPropertyService started!");
        }
        mHal = hal;
        mContext = context;
    }

    // Helper class to keep track of listeners to this service
    private class Client implements IBinder.DeathRecipient {
        private final ICarPropertyEventListener mListener;
        private final IBinder mListenerBinder;
        private final SparseArray<Float> mRateMap = new SparseArray<Float>();   // key is propId

        Client(ICarPropertyEventListener listener) {
            mListener = listener;
            mListenerBinder = listener.asBinder();

            try {
                mListenerBinder.linkToDeath(this, 0);
            } catch (RemoteException e) {
                throw new IllegalStateException("Client already dead", e);
            }
            mClientMap.put(mListenerBinder, this);
        }

        void addProperty(int propId, float rate) {
            mRateMap.put(propId, rate);
        }

        /**
         * Client died. Remove the listener from HAL service and unregister if this is the last
         * client.
         */
        @Override
        public void binderDied() {
            if (DBG) {
                Log.d(TAG, "binderDied " + mListenerBinder);
            }

            for (int i = 0; i < mRateMap.size(); i++) {
                int propId = mRateMap.keyAt(i);
                CarPropertyService.this.unregisterListenerBinderLocked(propId, mListenerBinder);
            }
            this.release();
        }

        ICarPropertyEventListener getListener() {
            return mListener;
        }

        IBinder getListenerBinder() {
            return mListenerBinder;
        }

        float getRate(int propId) {
            // Return 0 if no key found, since that is the slowest rate.
            return mRateMap.get(propId, (float) 0);
        }

        void release() {
            mListenerBinder.unlinkToDeath(this, 0);
            mClientMap.remove(mListenerBinder);
        }

        void removeProperty(int propId) {
            mRateMap.remove(propId);
            if (mRateMap.size() == 0) {
                // Last property was released, remove the client.
                this.release();
            }
        }
    }

    @Override
    public void init() {
        synchronized (mLock) {
            // Cache the configs list to avoid subsequent binder calls
            mConfigs.clear();
            mConfigs.putAll(mHal.getPropertyList());
        }
        if (DBG) {
            Log.d(TAG, "cache CarPropertyConfigs " + mConfigs.size());
        }
    }

    @Override
    public void release() {
        for (Client c : mClientMap.values()) {
            c.release();
        }
        mClientMap.clear();
        mPropIdClientMap.clear();
        mHal.setListener(null);
        mListenerIsSet = false;
        synchronized (mLock) {
            mSetOperationClientMap.clear();
        }
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*CarPropertyService*");
        synchronized (mLock) {
            writer.println("    Listener is set for PropertyHalService: " + mListenerIsSet);
            writer.println("    There are " + mClientMap.size() + " clients "
                    + "using CarPropertyService.");
            writer.println("    Properties registered: ");
            for (int propId : mPropIdClientMap.keySet()) {
                writer.println("        propId: 0x" + toHexString(propId)
                        + " is registered by " + mPropIdClientMap.get(propId).size()
                        + " client(s).");
            }
            writer.println("    Properties changed by CarPropertyService: ");
            for (int i = 0; i < mSetOperationClientMap.size(); i++) {
                int propId = mSetOperationClientMap.keyAt(i);
                SparseArray areaIdToClient = mSetOperationClientMap.valueAt(i);
                for (int j = 0; j < areaIdToClient.size(); j++) {
                    int areaId = areaIdToClient.keyAt(j);
                    writer.println("        propId: 0x" + toHexString(propId)
                            + " areaId: 0x" + toHexString(areaId)
                            + " by client: " + areaIdToClient.valueAt(j));
                }
            }
        }
    }

    @Override
    public void registerListener(int propId, float rate, ICarPropertyEventListener listener) {
        if (DBG) {
            Log.d(TAG, "registerListener: propId=0x" + toHexString(propId) + " rate=" + rate);
        }
        if (listener == null) {
            Log.e(TAG, "registerListener: Listener is null.");
            throw new IllegalArgumentException("listener cannot be null.");
        }

        IBinder listenerBinder = listener.asBinder();
        CarPropertyConfig propertyConfig;
        Client finalClient;
        synchronized (mLock) {
            propertyConfig = mConfigs.get(propId);
            if (propertyConfig == null) {
                // Do not attempt to register an invalid propId
                Log.e(TAG, "registerListener:  propId is not in config list: 0x" + toHexString(
                        propId));
                return;
            }
            ICarImpl.assertPermission(mContext, mHal.getReadPermission(propId));
            // Get or create the client for this listener
            Client client = mClientMap.get(listenerBinder);
            if (client == null) {
                client = new Client(listener);
            }
            client.addProperty(propId, rate);
            // Insert the client into the propId --> clients map
            List<Client> clients = mPropIdClientMap.get(propId);
            if (clients == null) {
                clients = new CopyOnWriteArrayList<Client>();
                mPropIdClientMap.put(propId, clients);
            }
            if (!clients.contains(client)) {
                clients.add(client);
            }
            // Set the HAL listener if necessary
            if (!mListenerIsSet) {
                mHal.setListener(this);
            }
            // Set the new rate
            if (rate > mHal.getSampleRate(propId)) {
                mHal.subscribeProperty(propId, rate);
            }
            finalClient = client;
        }

        // propertyConfig and client are NonNull.
        mHandler.post(() ->
                getAndDispatchPropertyInitValue(propertyConfig, finalClient));
    }

    private void getAndDispatchPropertyInitValue(CarPropertyConfig config, Client client) {
        List<CarPropertyEvent> events = new LinkedList<>();
        int propId = config.getPropertyId();
        if (config.isGlobalProperty()) {
            CarPropertyValue value = mHal.getProperty(propId, 0);
            if (value != null) {
                CarPropertyEvent event = new CarPropertyEvent(
                        CarPropertyEvent.PROPERTY_EVENT_PROPERTY_CHANGE, value);
                events.add(event);
            }
        } else {
            for (int areaId : config.getAreaIds()) {
                CarPropertyValue value = mHal.getProperty(propId, areaId);
                if (value != null) {
                    CarPropertyEvent event = new CarPropertyEvent(
                            CarPropertyEvent.PROPERTY_EVENT_PROPERTY_CHANGE, value);
                    events.add(event);
                }
            }
        }
        try {
            client.getListener().onEvent(events);
        } catch (RemoteException ex) {
            // If we cannot send a record, its likely the connection snapped. Let the binder
            // death handle the situation.
            Log.e(TAG, "onEvent calling failed: " + ex);
        }
    }

    @Override
    public void unregisterListener(int propId, ICarPropertyEventListener listener) {
        if (DBG) {
            Log.d(TAG, "unregisterListener propId=0x" + toHexString(propId));
        }
        ICarImpl.assertPermission(mContext, mHal.getReadPermission(propId));
        if (listener == null) {
            Log.e(TAG, "unregisterListener: Listener is null.");
            throw new IllegalArgumentException("Listener is null");
        }

        IBinder listenerBinder = listener.asBinder();
        synchronized (mLock) {
            unregisterListenerBinderLocked(propId, listenerBinder);
        }
    }

    private void unregisterListenerBinderLocked(int propId, IBinder listenerBinder) {
        Client client = mClientMap.get(listenerBinder);
        List<Client> propertyClients = mPropIdClientMap.get(propId);
        synchronized (mLock) {
            if (mConfigs.get(propId) == null) {
                // Do not attempt to register an invalid propId
                Log.e(TAG, "unregisterListener: propId is not in config list:0x" + toHexString(
                        propId));
                return;
            }
        }
        if ((client == null) || (propertyClients == null)) {
            Log.e(TAG, "unregisterListenerBinderLocked: Listener was not previously registered.");
        } else {
            if (propertyClients.remove(client)) {
                client.removeProperty(propId);
                clearSetOperationRecorderLocked(propId, client);
            } else {
                Log.e(TAG, "unregisterListenerBinderLocked: Listener was not registered for "
                           + "propId=0x" + toHexString(propId));
            }

            if (propertyClients.isEmpty()) {
                // Last listener for this property unsubscribed.  Clean up
                mHal.unsubscribeProperty(propId);
                mPropIdClientMap.remove(propId);
                mSetOperationClientMap.remove(propId);
                if (mPropIdClientMap.isEmpty()) {
                    // No more properties are subscribed.  Turn off the listener.
                    mHal.setListener(null);
                    mListenerIsSet = false;
                }
            } else {
                // Other listeners are still subscribed.  Calculate the new rate
                float maxRate = 0;
                for (Client c : propertyClients) {
                    float rate = c.getRate(propId);
                    if (rate > maxRate) {
                        maxRate = rate;
                    }
                }
                // Set the new rate
                mHal.subscribeProperty(propId, maxRate);
            }
        }
    }

    /**
     * Return the list of properties that the caller may access.
     */
    @Override
    public List<CarPropertyConfig> getPropertyList() {
        List<CarPropertyConfig> returnList = new ArrayList<CarPropertyConfig>();
        Set<CarPropertyConfig> allConfigs;
        synchronized (mLock) {
            allConfigs = new HashSet<>(mConfigs.values());
        }
        for (CarPropertyConfig c : allConfigs) {
            if (ICarImpl.hasPermission(mContext, mHal.getReadPermission(c.getPropertyId()))) {
                // Only add properties the list if the process has permissions to read it
                returnList.add(c);
            }
        }
        if (DBG) {
            Log.d(TAG, "getPropertyList returns " + returnList.size() + " configs");
        }
        return returnList;
    }

    @Override
    public CarPropertyValue getProperty(int prop, int zone) {
        synchronized (mLock) {
            if (mConfigs.get(prop) == null) {
                // Do not attempt to register an invalid propId
                Log.e(TAG, "getProperty: propId is not in config list:0x" + toHexString(prop));
                return null;
            }
        }
        ICarImpl.assertPermission(mContext, mHal.getReadPermission(prop));
        return mHal.getProperty(prop, zone);
    }

    @Override
    public String getReadPermission(int propId) {
        synchronized (mLock) {
            if (mConfigs.get(propId) == null) {
                // Property ID does not exist
                Log.e(TAG,
                        "getReadPermission: propId is not in config list:0x" + toHexString(propId));
                return null;
            }
        }
        return mHal.getReadPermission(propId);
    }

    @Override
    public String getWritePermission(int propId) {
        synchronized (mLock) {
            if (mConfigs.get(propId) == null) {
                // Property ID does not exist
                Log.e(TAG, "getWritePermission: propId is not in config list:0x" + toHexString(
                        propId));
                return null;
            }
        }
        return mHal.getWritePermission(propId);
    }

    @Override
    public void setProperty(CarPropertyValue prop, ICarPropertyEventListener listener) {
        int propId = prop.getPropertyId();
        checkPropertyAccessibility(propId);
        // need an extra permission for writing display units properties.
        if (mHal.isDisplayUnitsProperty(propId)) {
            ICarImpl.assertPermission(mContext, Car.PERMISSION_VENDOR_EXTENSION);
        }
        mHal.setProperty(prop);
        IBinder listenerBinder = listener.asBinder();
        synchronized (mLock) {
            Client client = mClientMap.get(listenerBinder);
            if (client == null) {
                client = new Client(listener);
            }
            updateSetOperationRecorder(propId, prop.getAreaId(), client);
        }
    }

    // The helper method checks if the vehicle has implemented this property and the property
    // is accessible or not for platform and client.
    private void checkPropertyAccessibility(int propId) {
        // Checks if the car implemented the property or not.
        synchronized (mLock) {
            if (mConfigs.get(propId) == null) {
                throw new IllegalArgumentException("Property Id: 0x" + Integer.toHexString(propId)
                        + " does not exist in the vehicle");
            }
        }

        // Checks if android has permission to write property.
        String propertyWritePermission = mHal.getWritePermission(propId);
        if (propertyWritePermission == null) {
            throw new SecurityException("Platform does not have permission to change value for "
                    + "property Id: 0x" + Integer.toHexString(propId));
        }
        // Checks if the client has the permission.
        ICarImpl.assertPermission(mContext, propertyWritePermission);
    }

    // Updates recorder for set operation.
    private void updateSetOperationRecorder(int propId, int areaId, Client client) {
        if (mSetOperationClientMap.get(propId) != null) {
            mSetOperationClientMap.get(propId).put(areaId, client);
        } else {
            SparseArray<Client> areaIdToClient = new SparseArray<>();
            areaIdToClient.put(areaId, client);
            mSetOperationClientMap.put(propId, areaIdToClient);
        }
    }

    // Clears map when client unregister for property.
    private void clearSetOperationRecorderLocked(int propId, Client client) {
        SparseArray<Client> areaIdToClient = mSetOperationClientMap.get(propId);
        if (areaIdToClient != null) {
            List<Integer> indexNeedToRemove = new ArrayList<>();
            for (int index = 0; index < areaIdToClient.size(); index++) {
                if (client.equals(areaIdToClient.valueAt(index))) {
                    indexNeedToRemove.add(index);
                }
            }

            for (int index : indexNeedToRemove) {
                if (DBG) {
                    Log.d("ErrorEvent", " Clear propId:0x" + toHexString(propId)
                            + " areaId: 0x" + toHexString(areaIdToClient.keyAt(index)));
                }
                areaIdToClient.removeAt(index);
            }
        }
    }

    // Implement PropertyHalListener interface
    @Override
    public void onPropertyChange(List<CarPropertyEvent> events) {
        Map<IBinder, Pair<ICarPropertyEventListener, List<CarPropertyEvent>>> eventsToDispatch =
                new HashMap<>();

        for (CarPropertyEvent event : events) {
            int propId = event.getCarPropertyValue().getPropertyId();
            List<Client> clients = mPropIdClientMap.get(propId);
            if (clients == null) {
                Log.e(TAG, "onPropertyChange: no listener registered for propId=0x"
                        + toHexString(propId));
                continue;
            }

            for (Client c : clients) {
                IBinder listenerBinder = c.getListenerBinder();
                Pair<ICarPropertyEventListener, List<CarPropertyEvent>> p =
                        eventsToDispatch.get(listenerBinder);
                if (p == null) {
                    // Initialize the linked list for the listener
                    p = new Pair<>(c.getListener(), new LinkedList<CarPropertyEvent>());
                    eventsToDispatch.put(listenerBinder, p);
                }
                p.second.add(event);
            }
        }
        // Parse the dispatch list to send events
        for (Pair<ICarPropertyEventListener, List<CarPropertyEvent>> p: eventsToDispatch.values()) {
            try {
                p.first.onEvent(p.second);
            } catch (RemoteException ex) {
                // If we cannot send a record, its likely the connection snapped. Let binder
                // death handle the situation.
                Log.e(TAG, "onEvent calling failed: " + ex);
            }
        }
    }

    @Override
    public void onPropertySetError(int property, int areaId, int errorCode) {
        Client lastOperatedClient = null;
        synchronized (mLock) {
            if (mSetOperationClientMap.get(property) != null
                    && mSetOperationClientMap.get(property).get(areaId) != null) {
                lastOperatedClient = mSetOperationClientMap.get(property).get(areaId);
            } else {
                Log.e(TAG, "Can not find the client changed propertyId: 0x"
                        + toHexString(property) + " in areaId: 0x" + toHexString(areaId));
            }

        }
        if (lastOperatedClient != null) {
            dispatchToLastClient(property, areaId, errorCode, lastOperatedClient);
        }
    }

    private void dispatchToLastClient(int property, int areaId, int errorCode,
            Client lastOperatedClient) {
        try {
            List<CarPropertyEvent> eventList = new LinkedList<>();
            eventList.add(
                    CarPropertyEvent.createErrorEventWithErrorCode(property, areaId,
                            errorCode));
            lastOperatedClient.getListener().onEvent(eventList);
        } catch (RemoteException ex) {
            Log.e(TAG, "onEvent calling failed: " + ex);
        }
    }
}
