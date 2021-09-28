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

import static android.car.drivingstate.CarDrivingStateEvent.DRIVING_STATE_IDLING;
import static android.car.drivingstate.CarDrivingStateEvent.DRIVING_STATE_MOVING;
import static android.car.drivingstate.CarDrivingStateEvent.DRIVING_STATE_PARKED;
import static android.car.drivingstate.CarDrivingStateEvent.DRIVING_STATE_UNKNOWN;
import static android.car.drivingstate.CarUxRestrictionsManager.UX_RESTRICTION_MODE_BASELINE;

import static java.nio.file.StandardCopyOption.REPLACE_EXISTING;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.car.Car;
import android.car.drivingstate.CarDrivingStateEvent;
import android.car.drivingstate.CarDrivingStateEvent.CarDrivingState;
import android.car.drivingstate.CarUxRestrictions;
import android.car.drivingstate.CarUxRestrictionsConfiguration;
import android.car.drivingstate.CarUxRestrictionsManager;
import android.car.drivingstate.ICarDrivingStateChangeListener;
import android.car.drivingstate.ICarUxRestrictionsChangeListener;
import android.car.drivingstate.ICarUxRestrictionsManager;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyEvent;
import android.car.hardware.property.ICarPropertyEventListener;
import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.hardware.display.DisplayManager;
import android.os.Binder;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IRemoteCallback;
import android.os.Process;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.os.SystemClock;
import android.util.ArraySet;
import android.util.AtomicFile;
import android.util.JsonReader;
import android.util.JsonToken;
import android.util.JsonWriter;
import android.util.Log;
import android.util.Slog;
import android.util.SparseArray;
import android.view.Display;
import android.view.DisplayAddress;

import com.android.car.systeminterface.SystemInterface;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import org.xmlpull.v1.XmlPullParserException;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/**
 * A service that listens to current driving state of the vehicle and maps it to the
 * appropriate UX restrictions for that driving state.
 * <p>
 * <h1>UX Restrictions Configuration</h1>
 * When this service starts, it will first try reading the configuration set through
 * {@link #saveUxRestrictionsConfigurationForNextBoot(List)}.
 * If one is not available, it will try reading the configuration saved in
 * {@code R.xml.car_ux_restrictions_map}. If XML is somehow unavailable, it will
 * fall back to a hard-coded configuration.
 * <p>
 * <h1>Multi-Display</h1>
 * Only physical displays that are available at service initialization are recognized.
 * This service does not support pluggable displays.
 */
public class CarUxRestrictionsManagerService extends ICarUxRestrictionsManager.Stub implements
        CarServiceBase {
    private static final String TAG = "CarUxR";
    private static final boolean DBG = false;
    private static final int MAX_TRANSITION_LOG_SIZE = 20;
    private static final int PROPERTY_UPDATE_RATE = 5; // Update rate in Hz
    private static final float SPEED_NOT_AVAILABLE = -1.0F;

    private static final int UNKNOWN_JSON_SCHEMA_VERSION = -1;
    private static final int JSON_SCHEMA_VERSION_V1 = 1;
    private static final int JSON_SCHEMA_VERSION_V2 = 2;

    @IntDef({UNKNOWN_JSON_SCHEMA_VERSION, JSON_SCHEMA_VERSION_V1, JSON_SCHEMA_VERSION_V2})
    @Retention(RetentionPolicy.SOURCE)
    private @interface JsonSchemaVersion {}

    private static final String JSON_NAME_SCHEMA_VERSION = "schema_version";
    private static final String JSON_NAME_RESTRICTIONS = "restrictions";
    private static final byte DEFAULT_PORT = 0;

    @VisibleForTesting
    static final String CONFIG_FILENAME_PRODUCTION = "ux_restrictions_prod_config.json";
    @VisibleForTesting
    static final String CONFIG_FILENAME_STAGED = "ux_restrictions_staged_config.json";

    private final Context mContext;
    private final DisplayManager mDisplayManager;
    private final CarDrivingStateService mDrivingStateService;
    private final CarPropertyService mCarPropertyService;
    private final HandlerThread mClientDispatchThread  = CarServiceUtils.getHandlerThread(
            getClass().getSimpleName());
    private final Handler mClientDispatchHandler  = new Handler(mClientDispatchThread.getLooper());
    private final RemoteCallbackList<ICarUxRestrictionsChangeListener> mUxRClients =
            new RemoteCallbackList<>();

    /**
     * Metadata associated with a binder callback.
     */
    private static class RemoteCallbackListCookie {
        final Byte mPhysicalPort;

        RemoteCallbackListCookie(Byte physicalPort) {
            mPhysicalPort = physicalPort;
        }
    }

    private final Object mLock = new Object();

    /**
     * This lookup caches the mapping from an int display id to a byte that represents a physical
     * port. It includes mappings for virtual displays.
     */
    @GuardedBy("mLock")
    private final Map<Integer, Byte> mPortLookup = new HashMap<>();

    @GuardedBy("mLock")
    private Map<Byte, CarUxRestrictionsConfiguration> mCarUxRestrictionsConfigurations;

    @GuardedBy("mLock")
    private Map<Byte, CarUxRestrictions> mCurrentUxRestrictions;

    @GuardedBy("mLock")
    private String mRestrictionMode = UX_RESTRICTION_MODE_BASELINE;

    @GuardedBy("mLock")
    private float mCurrentMovingSpeed;

    // Byte represents a physical port for display.
    @GuardedBy("mLock")
    private byte mDefaultDisplayPhysicalPort;

    @GuardedBy("mLock")
    private final List<Byte> mPhysicalPorts = new ArrayList<>();

    // Flag to disable broadcasting UXR changes - for development purposes
    @GuardedBy("mLock")
    private boolean mUxRChangeBroadcastEnabled = true;

    // For dumpsys logging
    @GuardedBy("mLock")
    private final LinkedList<Utils.TransitionLog> mTransitionLogs = new LinkedList<>();

    public CarUxRestrictionsManagerService(Context context, CarDrivingStateService drvService,
            CarPropertyService propertyService) {
        mContext = context;
        mDisplayManager = mContext.getSystemService(DisplayManager.class);
        mDrivingStateService = drvService;
        mCarPropertyService = propertyService;
    }

    @Override
    public void init() {
        synchronized (mLock) {
            mDefaultDisplayPhysicalPort = getDefaultDisplayPhysicalPort(mDisplayManager);
            initPhysicalPort();

            // Unrestricted until driving state information is received. During boot up, we don't
            // want
            // everything to be blocked until data is available from CarPropertyManager.  If we
            // start
            // driving and we don't get speed or gear information, we have bigger problems.
            mCurrentUxRestrictions = new HashMap<>();
            for (byte port : mPhysicalPorts) {
                mCurrentUxRestrictions.put(port, createUnrestrictedRestrictions());
            }

            // Load the prod config, or if there is a staged one, promote that first only if the
            // current driving state, as provided by the driving state service, is parked.
            mCarUxRestrictionsConfigurations = convertToMap(loadConfig());
        }

        // subscribe to driving state changes
        mDrivingStateService.registerDrivingStateChangeListener(
                mICarDrivingStateChangeEventListener);
        // subscribe to property service for speed
        mCarPropertyService.registerListener(VehicleProperty.PERF_VEHICLE_SPEED,
                PROPERTY_UPDATE_RATE, mICarPropertyEventListener);

        initializeUxRestrictions();
    }

    @Override
    public List<CarUxRestrictionsConfiguration> getConfigs() {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_UX_RESTRICTIONS_CONFIGURATION);
        synchronized (mLock) {
            return new ArrayList<>(mCarUxRestrictionsConfigurations.values());
        }
    }

    /**
     * Loads UX restrictions configurations and returns them.
     *
     * <p>Reads config from the following sources in order:
     * <ol>
     * <li>saved config set by {@link #saveUxRestrictionsConfigurationForNextBoot(List)};
     * <li>XML resource config from {@code R.xml.car_ux_restrictions_map};
     * <li>hardcoded default config.
     * </ol>
     *
     * This method attempts to promote staged config file, which requires getting the current
     * driving state.
     */
    @VisibleForTesting
    List<CarUxRestrictionsConfiguration> loadConfig() {
        promoteStagedConfig();
        List<CarUxRestrictionsConfiguration> configs;

        // Production config, if available, is the first choice.
        File prodConfig = getFile(CONFIG_FILENAME_PRODUCTION);
        if (prodConfig.exists()) {
            logd("Attempting to read production config");
            configs = readPersistedConfig(prodConfig);
            if (configs != null) {
                return configs;
            }
        }

        // XML config is the second choice.
        logd("Attempting to read config from XML resource");
        configs = readXmlConfig();
        if (configs != null) {
            return configs;
        }

        // This should rarely happen.
        Log.w(TAG, "Creating default config");

        configs = new ArrayList<>();
        synchronized (mLock) {
            for (byte port : mPhysicalPorts) {
                configs.add(createDefaultConfig(port));
            }
        }
        return configs;
    }

    private File getFile(String filename) {
        SystemInterface systemInterface = CarLocalServices.getService(SystemInterface.class);
        return new File(systemInterface.getSystemCarDir(), filename);
    }

    @Nullable
    private List<CarUxRestrictionsConfiguration> readXmlConfig() {
        try {
            return CarUxRestrictionsConfigurationXmlParser.parse(
                    mContext, R.xml.car_ux_restrictions_map);
        } catch (IOException | XmlPullParserException e) {
            Log.e(TAG, "Could not read config from XML resource", e);
        }
        return null;
    }

    /**
     * Promotes the staged config to prod, by replacing the prod file. Only do this if the car is
     * parked to avoid changing the restrictions during a drive.
     */
    private void promoteStagedConfig() {
        Path stagedConfig = getFile(CONFIG_FILENAME_STAGED).toPath();

        CarDrivingStateEvent currentDrivingStateEvent =
                mDrivingStateService.getCurrentDrivingState();
        // Only promote staged config when car is parked.
        if (currentDrivingStateEvent != null
                && currentDrivingStateEvent.eventValue == DRIVING_STATE_PARKED
                && Files.exists(stagedConfig)) {

            Path prod = getFile(CONFIG_FILENAME_PRODUCTION).toPath();
            try {
                logd("Attempting to promote stage config");
                Files.move(stagedConfig, prod, REPLACE_EXISTING);
            } catch (IOException e) {
                Log.e(TAG, "Could not promote state config", e);
            }
        }
    }

    // Update current restrictions by getting the current driving state and speed.
    private void initializeUxRestrictions() {
        CarDrivingStateEvent currentDrivingStateEvent =
                mDrivingStateService.getCurrentDrivingState();
        // if we don't have enough information from the CarPropertyService to compute the UX
        // restrictions, then leave the UX restrictions unchanged from what it was initialized to
        // in the constructor.
        if (currentDrivingStateEvent == null
                || currentDrivingStateEvent.eventValue == DRIVING_STATE_UNKNOWN) {
            return;
        }
        int currentDrivingState = currentDrivingStateEvent.eventValue;
        Float currentSpeed = getCurrentSpeed();
        if (currentSpeed == SPEED_NOT_AVAILABLE) {
            return;
        }
        // At this point the underlying CarPropertyService has provided us enough information to
        // compute the UX restrictions that could be potentially different from the initial UX
        // restrictions.
        synchronized (mLock) {
            handleDispatchUxRestrictionsLocked(currentDrivingState, currentSpeed);
        }
    }

    private Float getCurrentSpeed() {
        CarPropertyValue value = mCarPropertyService.getProperty(VehicleProperty.PERF_VEHICLE_SPEED,
                0);
        if (value != null) {
            return (Float) value.getValue();
        }
        return SPEED_NOT_AVAILABLE;
    }

    @Override
    public void release() {
        while (mUxRClients.getRegisteredCallbackCount() > 0) {
            for (int i = mUxRClients.getRegisteredCallbackCount() - 1; i >= 0; i--) {
                ICarUxRestrictionsChangeListener client = mUxRClients.getRegisteredCallbackItem(i);
                if (client == null) {
                    continue;
                }
                mUxRClients.unregister(client);
            }
        }
        mDrivingStateService.unregisterDrivingStateChangeListener(
                mICarDrivingStateChangeEventListener);
        synchronized (mLock) {
            mActivityViewDisplayInfoMap.clear();
        }
    }

    // Binder methods

    /**
     * Registers a {@link ICarUxRestrictionsChangeListener} to be notified for changes to the UX
     * restrictions.
     *
     * @param listener  Listener to register
     * @param displayId UX restrictions on this display will be notified.
     */
    @Override
    public void registerUxRestrictionsChangeListener(
            ICarUxRestrictionsChangeListener listener, int displayId) {
        if (listener == null) {
            Log.e(TAG, "registerUxRestrictionsChangeListener(): listener null");
            throw new IllegalArgumentException("Listener is null");
        }
        Byte physicalPort;
        synchronized (mLock) {
            physicalPort = getPhysicalPortLocked(displayId);
            if (physicalPort == null) {
                physicalPort = mDefaultDisplayPhysicalPort;
            }
        }
        mUxRClients.register(listener, new RemoteCallbackListCookie(physicalPort));
    }

    /**
     * Unregister the given UX Restrictions listener
     *
     * @param listener client to unregister
     */
    @Override
    public void unregisterUxRestrictionsChangeListener(ICarUxRestrictionsChangeListener listener) {
        if (listener == null) {
            Log.e(TAG, "unregisterUxRestrictionsChangeListener(): listener null");
            throw new IllegalArgumentException("Listener is null");
        }

        mUxRClients.unregister(listener);
    }

    /**
     * Gets the current UX restrictions for a display.
     *
     * @param displayId UX restrictions on this display will be returned.
     */
    @Override
    public CarUxRestrictions getCurrentUxRestrictions(int displayId) {
        CarUxRestrictions restrictions;
        synchronized (mLock) {
            restrictions = mCurrentUxRestrictions.get(getPhysicalPortLocked(displayId));
        }
        if (restrictions == null) {
            Log.e(TAG, String.format(
                    "Restrictions are null for displayId:%d. Returning full restrictions.",
                    displayId));
            restrictions = createFullyRestrictedRestrictions();
        }
        return restrictions;
    }

    /**
     * Convenience method to retrieve restrictions for default display.
     */
    @Nullable
    public CarUxRestrictions getCurrentUxRestrictions() {
        return getCurrentUxRestrictions(Display.DEFAULT_DISPLAY);
    }

    @Override
    public boolean saveUxRestrictionsConfigurationForNextBoot(
            List<CarUxRestrictionsConfiguration> configs) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_UX_RESTRICTIONS_CONFIGURATION);

        validateConfigs(configs);

        return persistConfig(configs, CONFIG_FILENAME_STAGED);
    }

    @Override
    @Nullable
    public List<CarUxRestrictionsConfiguration> getStagedConfigs() {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_UX_RESTRICTIONS_CONFIGURATION);

        File stagedConfig = getFile(CONFIG_FILENAME_STAGED);
        if (stagedConfig.exists()) {
            logd("Attempting to read staged config");
            return readPersistedConfig(stagedConfig);
        } else {
            return null;
        }
    }

    /**
     * Sets the restriction mode to use. Restriction mode allows a different set of restrictions to
     * be applied in the same driving state. Restrictions for each mode can be configured through
     * {@link CarUxRestrictionsConfiguration}.
     *
     * <p>Defaults to {@link CarUxRestrictionsManager#UX_RESTRICTION_MODE_BASELINE}.
     *
     * @param mode the restriction mode
     * @return {@code true} if mode was successfully changed; {@code false} otherwise.
     * @see CarUxRestrictionsConfiguration.DrivingStateRestrictions
     * @see CarUxRestrictionsConfiguration.Builder
     */
    @Override
    public boolean setRestrictionMode(@NonNull String mode) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_UX_RESTRICTIONS_CONFIGURATION);
        Objects.requireNonNull(mode, "mode must not be null");

        synchronized (mLock) {
            if (mRestrictionMode.equals(mode)) {
                return true;
            }

            addTransitionLogLocked(TAG, mRestrictionMode, mode, System.currentTimeMillis(),
                    "Restriction mode");
            mRestrictionMode = mode;
            logd("Set restriction mode to: " + mode);

            handleDispatchUxRestrictionsLocked(
                    mDrivingStateService.getCurrentDrivingState().eventValue, getCurrentSpeed());
        }
        return true;
    }

    @Override
    @NonNull
    public String getRestrictionMode() {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_CAR_UX_RESTRICTIONS_CONFIGURATION);

        synchronized (mLock) {
            return mRestrictionMode;
        }
    }

    /**
     * Writes configuration into the specified file.
     *
     * IO access on file is not thread safe. Caller should ensure threading protection.
     */
    private boolean persistConfig(List<CarUxRestrictionsConfiguration> configs, String filename) {
        File file = getFile(filename);
        AtomicFile stagedFile = new AtomicFile(file);
        FileOutputStream fos;
        try {
            fos = stagedFile.startWrite();
        } catch (IOException e) {
            Log.e(TAG, "Could not open file to persist config", e);
            return false;
        }
        try (JsonWriter jsonWriter = new JsonWriter(
                new OutputStreamWriter(fos, StandardCharsets.UTF_8))) {
            writeJson(jsonWriter, configs);
        } catch (IOException e) {
            Log.e(TAG, "Could not persist config", e);
            stagedFile.failWrite(fos);
            return false;
        }
        stagedFile.finishWrite(fos);
        return true;
    }

    @VisibleForTesting
    void writeJson(JsonWriter jsonWriter, List<CarUxRestrictionsConfiguration> configs)
            throws IOException {
        jsonWriter.beginObject();
        jsonWriter.name(JSON_NAME_SCHEMA_VERSION).value(JSON_SCHEMA_VERSION_V2);
        jsonWriter.name(JSON_NAME_RESTRICTIONS);
        jsonWriter.beginArray();
        for (CarUxRestrictionsConfiguration config : configs) {
            config.writeJson(jsonWriter);
        }
        jsonWriter.endArray();
        jsonWriter.endObject();
    }

    @Nullable
    private List<CarUxRestrictionsConfiguration> readPersistedConfig(File file) {
        if (!file.exists()) {
            Log.e(TAG, "Could not find config file: " + file.getName());
            return null;
        }

        // Take one pass at the file to check the version and then a second pass to read the
        // contents. We could assess the version and read in one pass, but we're preferring
        // clarity over complexity here.
        int schemaVersion = readFileSchemaVersion(file);

        AtomicFile configFile = new AtomicFile(file);
        try (JsonReader reader = new JsonReader(
                new InputStreamReader(configFile.openRead(), StandardCharsets.UTF_8))) {
            List<CarUxRestrictionsConfiguration> configs = new ArrayList<>();
            switch (schemaVersion) {
                case JSON_SCHEMA_VERSION_V1:
                    readV1Json(reader, configs);
                    break;
                case JSON_SCHEMA_VERSION_V2:
                    readV2Json(reader, configs);
                    break;
                default:
                    Log.e(TAG, "Unable to parse schema for version " + schemaVersion);
            }

            return configs;
        } catch (IOException e) {
            Log.e(TAG, "Could not read persisted config file " + file.getName(), e);
        }
        return null;
    }

    private void readV1Json(JsonReader reader,
            List<CarUxRestrictionsConfiguration> configs) throws IOException {
        readRestrictionsArray(reader, configs, JSON_SCHEMA_VERSION_V1);
    }

    private void readV2Json(JsonReader reader,
            List<CarUxRestrictionsConfiguration> configs) throws IOException {
        reader.beginObject();
        while (reader.hasNext()) {
            String name = reader.nextName();
            switch (name) {
                case JSON_NAME_RESTRICTIONS:
                    readRestrictionsArray(reader, configs, JSON_SCHEMA_VERSION_V2);
                    break;
                default:
                    reader.skipValue();
            }
        }
        reader.endObject();
    }

    private int readFileSchemaVersion(File file) {
        AtomicFile configFile = new AtomicFile(file);
        try (JsonReader reader = new JsonReader(
                new InputStreamReader(configFile.openRead(), StandardCharsets.UTF_8))) {
            List<CarUxRestrictionsConfiguration> configs = new ArrayList<>();
            if (reader.peek() == JsonToken.BEGIN_ARRAY) {
                // only schema V1 beings with an array - no need to keep reading
                reader.close();
                return JSON_SCHEMA_VERSION_V1;
            } else {
                reader.beginObject();
                while (reader.hasNext()) {
                    String name = reader.nextName();
                    switch (name) {
                        case JSON_NAME_SCHEMA_VERSION:
                            int schemaVersion = reader.nextInt();
                            // got the version, no need to continue reading
                            reader.close();
                            return schemaVersion;
                        default:
                            reader.skipValue();
                    }
                }
                reader.endObject();
            }
        } catch (IOException e) {
            Log.e(TAG, "Could not read persisted config file " + file.getName(), e);
        }
        return UNKNOWN_JSON_SCHEMA_VERSION;
    }

    private void readRestrictionsArray(JsonReader reader,
            List<CarUxRestrictionsConfiguration> configs, @JsonSchemaVersion int schemaVersion)
            throws IOException {
        reader.beginArray();
        while (reader.hasNext()) {
            configs.add(CarUxRestrictionsConfiguration.readJson(reader, schemaVersion));
        }
        reader.endArray();
    }

    /**
     * Enable/disable UX restrictions change broadcast blocking.
     * Setting this to true will stop broadcasts of UX restriction change to listeners.
     * This method works only on debug builds and the caller of this method needs to have the same
     * signature of the car service.
     */
    public void setUxRChangeBroadcastEnabled(boolean enable) {
        if (!isDebugBuild()) {
            Log.e(TAG, "Cannot set UX restriction change broadcast.");
            return;
        }
        // Check if the caller has the same signature as that of the car service.
        if (mContext.getPackageManager().checkSignatures(Process.myUid(), Binder.getCallingUid())
                != PackageManager.SIGNATURE_MATCH) {
            throw new SecurityException(
                    "Caller " + mContext.getPackageManager().getNameForUid(Binder.getCallingUid())
                            + " does not have the right signature");
        }

        synchronized (mLock) {
            if (enable) {
                // if enabling it back, send the current restrictions
                mUxRChangeBroadcastEnabled = enable;
                handleDispatchUxRestrictionsLocked(
                        mDrivingStateService.getCurrentDrivingState().eventValue,
                        getCurrentSpeed());
            } else {
                // fake parked state, so if the system is currently restricted, the restrictions are
                // relaxed.
                handleDispatchUxRestrictionsLocked(DRIVING_STATE_PARKED, 0);
                mUxRChangeBroadcastEnabled = enable;
            }
        }
    }

    private boolean isDebugBuild() {
        return Build.IS_USERDEBUG || Build.IS_ENG;
    }

    @Override
    public void dump(PrintWriter writer) {
        synchronized (mLock) {
            writer.println("*CarUxRestrictionsManagerService*");
            mUxRClients.dump(writer, "UX Restrictions Clients ");
            for (byte port : mCurrentUxRestrictions.keySet()) {
                CarUxRestrictions restrictions = mCurrentUxRestrictions.get(port);
                writer.printf("Port: 0x%02X UXR: %s\n", port, restrictions.toString());
            }
            if (isDebugBuild()) {
                writer.println("mUxRChangeBroadcastEnabled? " + mUxRChangeBroadcastEnabled);
            }
            writer.println("UX Restriction configurations:");
            for (CarUxRestrictionsConfiguration config :
                    mCarUxRestrictionsConfigurations.values()) {
                config.dump(writer);
            }
            writer.println("UX Restriction change log:");
            for (Utils.TransitionLog tlog : mTransitionLogs) {
                writer.println(tlog);
            }
            writer.println("UX Restriction display info:");
            for (int i = mActivityViewDisplayInfoMap.size() - 1; i >= 0; --i) {
                DisplayInfo info = mActivityViewDisplayInfoMap.valueAt(i);
                writer.printf("Display%d: physicalDisplayId=%d, owner=%s\n",
                        mActivityViewDisplayInfoMap.keyAt(i), info.mPhysicalDisplayId, info.mOwner);
            }
        }
    }

    /**
     * {@link CarDrivingStateEvent} listener registered with the {@link CarDrivingStateService}
     * for getting driving state change notifications.
     */
    private final ICarDrivingStateChangeListener mICarDrivingStateChangeEventListener =
            new ICarDrivingStateChangeListener.Stub() {
                @Override
                public void onDrivingStateChanged(CarDrivingStateEvent event) {
                    logd("Driving State Changed:" + event.eventValue);
                    synchronized (mLock) {
                        handleDrivingStateEventLocked(event);
                    }
                }
            };

    /**
     * Handle the driving state change events coming from the {@link CarDrivingStateService}.
     * Map the driving state to the corresponding UX Restrictions and dispatch the
     * UX Restriction change to the registered clients.
     */
    @VisibleForTesting
    @GuardedBy("mLock")
    void handleDrivingStateEventLocked(CarDrivingStateEvent event) {
        if (event == null) {
            return;
        }
        int drivingState = event.eventValue;
        Float speed = getCurrentSpeed();

        if (speed != SPEED_NOT_AVAILABLE) {
            mCurrentMovingSpeed = speed;
        } else if (drivingState == DRIVING_STATE_PARKED
                || drivingState == DRIVING_STATE_UNKNOWN) {
            // If speed is unavailable, but the driving state is parked or unknown, it can still be
            // handled.
            logd("Speed null when driving state is: " + drivingState);
            mCurrentMovingSpeed = 0;
        } else {
            // If we get here with driving state != parked or unknown && speed == null,
            // something is wrong.  CarDrivingStateService could not have inferred idling or moving
            // when speed is not available
            Log.e(TAG, "Unexpected:  Speed null when driving state is: " + drivingState);
            return;
        }
        handleDispatchUxRestrictionsLocked(drivingState, mCurrentMovingSpeed);
    }

    /**
     * {@link CarPropertyEvent} listener registered with the {@link CarPropertyService} for getting
     * speed change notifications.
     */
    private final ICarPropertyEventListener mICarPropertyEventListener =
            new ICarPropertyEventListener.Stub() {
                @Override
                public void onEvent(List<CarPropertyEvent> events) throws RemoteException {
                    synchronized (mLock) {
                        for (CarPropertyEvent event : events) {
                            if ((event.getEventType()
                                    == CarPropertyEvent.PROPERTY_EVENT_PROPERTY_CHANGE)
                                    && (event.getCarPropertyValue().getPropertyId()
                                    == VehicleProperty.PERF_VEHICLE_SPEED)) {
                                handleSpeedChangeLocked(
                                        (Float) event.getCarPropertyValue().getValue());
                            }
                        }
                    }
                }
            };

    @GuardedBy("mLock")
    private void handleSpeedChangeLocked(float newSpeed) {
        if (newSpeed == mCurrentMovingSpeed) {
            // Ignore if speed hasn't changed
            return;
        }
        int currentDrivingState = mDrivingStateService.getCurrentDrivingState().eventValue;
        if (currentDrivingState != DRIVING_STATE_MOVING) {
            // Ignore speed changes if the vehicle is not moving
            return;
        }
        mCurrentMovingSpeed = newSpeed;
        handleDispatchUxRestrictionsLocked(currentDrivingState, newSpeed);
    }

    /**
     * Handle dispatching UX restrictions change.
     *
     * @param currentDrivingState driving state of the vehicle
     * @param speed               speed of the vehicle
     */
    @GuardedBy("mLock")
    private void handleDispatchUxRestrictionsLocked(@CarDrivingState int currentDrivingState,
            float speed) {
        Objects.requireNonNull(mCarUxRestrictionsConfigurations,
                "mCarUxRestrictionsConfigurations must be initialized");
        Objects.requireNonNull(mCurrentUxRestrictions,
                "mCurrentUxRestrictions must be initialized");

        if (isDebugBuild() && !mUxRChangeBroadcastEnabled) {
            Log.d(TAG, "Not dispatching UX Restriction due to setting");
            return;
        }

        Map<Byte, CarUxRestrictions> newUxRestrictions = new HashMap<>();
        for (byte port : mPhysicalPorts) {
            CarUxRestrictionsConfiguration config = mCarUxRestrictionsConfigurations.get(port);
            if (config == null) {
                continue;
            }

            CarUxRestrictions uxRestrictions = config.getUxRestrictions(
                    currentDrivingState, speed, mRestrictionMode);
            logd(String.format("Display port 0x%02x\tDO old->new: %b -> %b",
                    port,
                    mCurrentUxRestrictions.get(port).isRequiresDistractionOptimization(),
                    uxRestrictions.isRequiresDistractionOptimization()));
            logd(String.format("Display port 0x%02x\tUxR old->new: 0x%x -> 0x%x",
                    port,
                    mCurrentUxRestrictions.get(port).getActiveRestrictions(),
                    uxRestrictions.getActiveRestrictions()));
            newUxRestrictions.put(port, uxRestrictions);
        }

        // Ignore dispatching if the restrictions has not changed.
        Set<Byte> displayToDispatch = new ArraySet<>();
        for (byte port : newUxRestrictions.keySet()) {
            if (!mCurrentUxRestrictions.containsKey(port)) {
                // This should never happen.
                Log.wtf(TAG, "Unrecognized port:" + port);
                continue;
            }
            CarUxRestrictions uxRestrictions = newUxRestrictions.get(port);
            if (!mCurrentUxRestrictions.get(port).isSameRestrictions(uxRestrictions)) {
                displayToDispatch.add(port);
            }
        }
        if (displayToDispatch.isEmpty()) {
            return;
        }

        for (byte port : displayToDispatch) {
            addTransitionLogLocked(
                    mCurrentUxRestrictions.get(port), newUxRestrictions.get(port));
        }

        dispatchRestrictionsToClients(newUxRestrictions, displayToDispatch);

        mCurrentUxRestrictions = newUxRestrictions;
    }

    private void dispatchRestrictionsToClients(Map<Byte, CarUxRestrictions> displayRestrictions,
            Set<Byte> displayToDispatch) {
        logd("dispatching to clients");
        boolean success = mClientDispatchHandler.post(() -> {
            int numClients = mUxRClients.beginBroadcast();
            for (int i = 0; i < numClients; i++) {
                ICarUxRestrictionsChangeListener callback = mUxRClients.getBroadcastItem(i);
                RemoteCallbackListCookie cookie =
                        (RemoteCallbackListCookie) mUxRClients.getBroadcastCookie(i);
                if (!displayToDispatch.contains(cookie.mPhysicalPort)) {
                    continue;
                }
                CarUxRestrictions restrictions = displayRestrictions.get(cookie.mPhysicalPort);
                if (restrictions == null) {
                    // don't dispatch to displays without configurations
                    continue;
                }
                try {
                    callback.onUxRestrictionsChanged(restrictions);
                } catch (RemoteException e) {
                    Log.e(TAG,
                            String.format("Dispatch to listener %s failed for restrictions (%s)",
                                    callback, restrictions));
                }
            }
            mUxRClients.finishBroadcast();
        });

        if (!success) {
            Log.e(TAG, String.format("Unable to post (%s) event to dispatch handler",
                    displayRestrictions));
        }
    }

    @VisibleForTesting
    static byte getDefaultDisplayPhysicalPort(DisplayManager displayManager) {
        Display defaultDisplay = displayManager.getDisplay(Display.DEFAULT_DISPLAY);
        DisplayAddress.Physical address = (DisplayAddress.Physical) defaultDisplay.getAddress();

        if (address == null) {
            Log.e(TAG, "Default display does not have physical display port. Using 0 as port.");
            return DEFAULT_PORT;
        }
        return address.getPort();
    }

    private void initPhysicalPort() {
        for (Display display : mDisplayManager.getDisplays()) {
            if (display.getType() == Display.TYPE_VIRTUAL) {
                continue;
            }

            if (display.getDisplayId() == Display.DEFAULT_DISPLAY && display.getAddress() == null) {
                // Assume default display is a physical display so assign an address if it
                // does not have one (possibly due to lower graphic driver version).
                if (Log.isLoggable(TAG, Log.INFO)) {
                    Log.i(TAG, "Default display does not have display address. Using default.");
                }
                synchronized (mLock) {
                    mPhysicalPorts.add(mDefaultDisplayPhysicalPort);
                }
            } else if (display.getAddress() instanceof DisplayAddress.Physical) {
                byte port = ((DisplayAddress.Physical) display.getAddress()).getPort();
                if (Log.isLoggable(TAG, Log.INFO)) {
                    Log.i(TAG, String.format(
                            "Display %d uses port %d", display.getDisplayId(),
                            Byte.toUnsignedInt(port)));
                }
                synchronized (mLock) {
                    mPhysicalPorts.add(port);
                }
            } else {
                Log.w(TAG, "At init non-virtual display has a non-physical display address: "
                        + display);
            }
        }
    }

    private Map<Byte, CarUxRestrictionsConfiguration> convertToMap(
            List<CarUxRestrictionsConfiguration> configs) {
        validateConfigs(configs);

        Map<Byte, CarUxRestrictionsConfiguration> result = new HashMap<>();
        if (configs.size() == 1) {
            CarUxRestrictionsConfiguration config = configs.get(0);
            synchronized (mLock) {
                byte port = config.getPhysicalPort() == null
                        ? mDefaultDisplayPhysicalPort
                        : config.getPhysicalPort();
                result.put(port, config);
            }
        } else {
            for (CarUxRestrictionsConfiguration config : configs) {
                result.put(config.getPhysicalPort(), config);
            }
        }
        return result;
    }

    /**
     * Validates configs for multi-display:
     * - share the same restrictions parameters;
     * - each sets display port;
     * - each has unique display port.
     */
    @VisibleForTesting
    void validateConfigs(List<CarUxRestrictionsConfiguration> configs) {
        if (configs.size() == 0) {
            throw new IllegalArgumentException("Empty configuration.");
        }

        if (configs.size() == 1) {
            return;
        }

        CarUxRestrictionsConfiguration first = configs.get(0);
        Set<Byte> existingPorts = new ArraySet<>();
        for (CarUxRestrictionsConfiguration config : configs) {
            if (!config.hasSameParameters(first)) {
                // Input should have the same restriction parameters because:
                // - it doesn't make sense otherwise; and
                // - in format it matches how xml can only specify one set of parameters.
                throw new IllegalArgumentException(
                        "Configurations should have the same restrictions parameters.");
            }

            Byte port = config.getPhysicalPort();
            if (port == null) {
                // Size was checked above; safe to assume there are multiple configs.
                throw new IllegalArgumentException(
                        "Input contains multiple configurations; each must set physical port.");
            }
            if (existingPorts.contains(port)) {
                throw new IllegalArgumentException("Multiple configurations for port " + port);
            }

            existingPorts.add(port);
        }
    }

    /**
     * Returns the physical port byte id for the display or {@code null} if {@link
     * DisplayManager#getDisplay(int)} is not aware of the provided id.
     */
    @Nullable
    @GuardedBy("mLock")
    private Byte getPhysicalPortLocked(int displayId) {
        if (!mPortLookup.containsKey(displayId)) {
            Display display = mDisplayManager.getDisplay(displayId);
            if (display == null) {
                Log.w(TAG, "Could not retrieve display for id: " + displayId);
                return null;
            }
            byte port = doGetPhysicalPortLocked(display);
            mPortLookup.put(displayId, port);
        }
        return mPortLookup.get(displayId);
    }

    @GuardedBy("mLock")
    private byte doGetPhysicalPortLocked(@NonNull Display display) {
        if (display.getType() == Display.TYPE_VIRTUAL) {
            Log.e(TAG, "Display " + display
                    + " is a virtual display and does not have a known port.");
            return mDefaultDisplayPhysicalPort;
        }

        DisplayAddress address = display.getAddress();
        if (address == null) {
            Log.e(TAG, "Display " + display
                    + " is not a virtual display but has null DisplayAddress.");
            return mDefaultDisplayPhysicalPort;
        } else if (!(address instanceof DisplayAddress.Physical)) {
            Log.e(TAG, "Display " + display + " has non-physical address: " + address);
            return mDefaultDisplayPhysicalPort;
        } else {
            return ((DisplayAddress.Physical) address).getPort();
        }
    }

    private CarUxRestrictions createUnrestrictedRestrictions() {
        return new CarUxRestrictions.Builder(/* reqOpt= */ false,
                CarUxRestrictions.UX_RESTRICTIONS_BASELINE, SystemClock.elapsedRealtimeNanos())
                .build();
    }

    private CarUxRestrictions createFullyRestrictedRestrictions() {
        return new CarUxRestrictions.Builder(
                /*reqOpt= */ true,
                CarUxRestrictions.UX_RESTRICTIONS_FULLY_RESTRICTED,
                SystemClock.elapsedRealtimeNanos()).build();
    }

    CarUxRestrictionsConfiguration createDefaultConfig(byte port) {
        return new CarUxRestrictionsConfiguration.Builder()
                .setPhysicalPort(port)
                .setUxRestrictions(DRIVING_STATE_PARKED,
                        false, CarUxRestrictions.UX_RESTRICTIONS_BASELINE)
                .setUxRestrictions(DRIVING_STATE_IDLING,
                        false, CarUxRestrictions.UX_RESTRICTIONS_BASELINE)
                .setUxRestrictions(DRIVING_STATE_MOVING,
                        true, CarUxRestrictions.UX_RESTRICTIONS_FULLY_RESTRICTED)
                .setUxRestrictions(DRIVING_STATE_UNKNOWN,
                        true, CarUxRestrictions.UX_RESTRICTIONS_FULLY_RESTRICTED)
                .build();
    }

    @GuardedBy("mLock")
    private void addTransitionLogLocked(String name, String from, String to, long timestamp,
            String extra) {
        if (mTransitionLogs.size() >= MAX_TRANSITION_LOG_SIZE) {
            mTransitionLogs.remove();
        }

        Utils.TransitionLog tLog = new Utils.TransitionLog(name, from, to, timestamp, extra);
        mTransitionLogs.add(tLog);
    }

    @GuardedBy("mLock")
    private void addTransitionLogLocked(
            CarUxRestrictions oldRestrictions, CarUxRestrictions newRestrictions) {
        if (mTransitionLogs.size() >= MAX_TRANSITION_LOG_SIZE) {
            mTransitionLogs.remove();
        }
        StringBuilder extra = new StringBuilder();
        extra.append(oldRestrictions.isRequiresDistractionOptimization() ? "DO -> " : "No DO -> ");
        extra.append(newRestrictions.isRequiresDistractionOptimization() ? "DO" : "No DO");

        Utils.TransitionLog tLog = new Utils.TransitionLog(TAG,
                oldRestrictions.getActiveRestrictions(), newRestrictions.getActiveRestrictions(),
                System.currentTimeMillis(), extra.toString());
        mTransitionLogs.add(tLog);
    }

    private static void logd(String msg) {
        if (DBG) {
            Slog.d(TAG, msg);
        }
    }

    private static final class DisplayInfo {
        final IRemoteCallback mOwner;
        final int mPhysicalDisplayId;

        DisplayInfo(IRemoteCallback owner, int physicalDisplayId) {
            mOwner = owner;
            mPhysicalDisplayId = physicalDisplayId;
        }
    }

    @GuardedBy("mLock")
    private final SparseArray<DisplayInfo> mActivityViewDisplayInfoMap = new SparseArray<>();

    @GuardedBy("mLock")
    private final RemoteCallbackList<IRemoteCallback> mRemoteCallbackList =
            new RemoteCallbackList<>() {
                @Override
                public void onCallbackDied(IRemoteCallback callback) {
                    synchronized (mLock) {
                        // Descending order to delete items safely from SpareArray.gc().
                        for (int i = mActivityViewDisplayInfoMap.size() - 1; i >= 0; --i) {
                            DisplayInfo info = mActivityViewDisplayInfoMap.valueAt(i);
                            if (info.mOwner == callback) {
                                logd("onCallbackDied: clean up callback=" + callback);
                                mActivityViewDisplayInfoMap.removeAt(i);
                                mPortLookup.remove(mActivityViewDisplayInfoMap.keyAt(i));
                            }
                        }
                    }
                }
            };

    @Override
    public void reportVirtualDisplayToPhysicalDisplay(IRemoteCallback callback,
            int virtualDisplayId, int physicalDisplayId) {
        logd("reportVirtualDisplayToPhysicalDisplay: callback=" + callback
                + ", virtualDisplayId=" + virtualDisplayId
                + ", physicalDisplayId=" + physicalDisplayId);
        boolean release = physicalDisplayId == Display.INVALID_DISPLAY;
        checkCallerOwnsDisplay(virtualDisplayId, release);
        synchronized (mLock) {
            if (release) {
                mRemoteCallbackList.unregister(callback);
                mActivityViewDisplayInfoMap.delete(virtualDisplayId);
                mPortLookup.remove(virtualDisplayId);
                return;
            }
            mRemoteCallbackList.register(callback);
            mActivityViewDisplayInfoMap.put(virtualDisplayId,
                    new DisplayInfo(callback, physicalDisplayId));
            Byte physicalPort = getPhysicalPortLocked(physicalDisplayId);
            if (physicalPort == null) {
                // This should not happen.
                Log.wtf(TAG, "No known physicalPort for displayId:" + physicalDisplayId);
                physicalPort = mDefaultDisplayPhysicalPort;
            }
            mPortLookup.put(virtualDisplayId, physicalPort);
        }
    }

    @Override
    public int getMappedPhysicalDisplayOfVirtualDisplay(int displayId) {
        logd("getMappedPhysicalDisplayOfVirtualDisplay: displayId=" + displayId);
        synchronized (mLock) {
            DisplayInfo foundInfo = mActivityViewDisplayInfoMap.get(displayId);
            if (foundInfo == null) {
                return Display.INVALID_DISPLAY;
            }
            // ActivityView can be placed in another ActivityView, so we should repeat the process
            // until no parent is found (reached to the physical display).
            while (foundInfo != null) {
                displayId = foundInfo.mPhysicalDisplayId;
                foundInfo = mActivityViewDisplayInfoMap.get(displayId);
            }
        }
        return displayId;
    }

    private void checkCallerOwnsDisplay(int displayId, boolean release) {
        Display display = mDisplayManager.getDisplay(displayId);
        if (display == null) {
            // Bypasses the permission check for non-existing display when releasing it, since
            // reportVirtualDisplayToPhysicalDisplay() and releasing display happens simultaneously
            // and it's no harm to release the information on the non-existing display.
            if (release) return;
            throw new IllegalArgumentException(
                    "Cannot find display for non-existent displayId: " + displayId);
        }

        int callingUid = Binder.getCallingUid();
        int displayOwnerUid = display.getOwnerUid();
        if (callingUid != displayOwnerUid) {
            throw new SecurityException("The caller doesn't own the display: callingUid="
                    + callingUid + ", displayOwnerUid=" + displayOwnerUid);
        }
    }
}
