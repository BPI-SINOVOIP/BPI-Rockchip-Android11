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
package com.google.android.chre.test.crossvalidator;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.hardware.location.ContextHubInfo;
import android.hardware.location.ContextHubManager;
import android.hardware.location.ContextHubTransaction;
import android.hardware.location.NanoAppBinary;
import android.hardware.location.NanoAppMessage;

import androidx.test.InstrumentationRegistry;

import com.google.android.chre.nanoapp.proto.ChreCrossValidationSensor;
import com.google.android.utils.chre.ChreTestUtil;
import com.google.common.collect.BiMap;
import com.google.common.collect.HashBiMap;
import com.google.protobuf.InvalidProtocolBufferException;

import org.junit.Assert;
import org.junit.Assume;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class ChreCrossValidatorSensor
        extends ChreCrossValidatorBase implements SensorEventListener {
    /**
    * Contains settings that can be adjusted per senor.
    */
    private static class CrossValidatorSensorConfig {
        // The number of float values expected in the values array of a datapoint
        public final int expectedValuesLength;
        // The amount that each value in the values array of a certain datapoint can differ between
        // AP and CHRE
        public final float errorMargin;

        CrossValidatorSensorConfig(int expectedValuesLength, float errorMargin) {
            this.expectedValuesLength = expectedValuesLength;
            this.errorMargin = errorMargin;
        }
    }

    private static final long NANO_APP_ID = 0x476f6f6754000002L;

    private static final long AWAIT_DATA_TIMEOUT_CONTINUOUS_IN_MS = 5000;
    private static final long AWAIT_DATA_TIMEOUT_ON_CHANGE_ONE_SHOT_IN_MS = 1000;
    private static final long INFO_RESPONSE_TIMEOUT_MS = 1000;

    private static final long DEFAULT_SAMPLING_INTERVAL_IN_MS = 20;

    private static final long SAMPLING_LATENCY_IN_MS = 0;

    private static final long MAX_TIMESTAMP_DIFF_NS = 10000000L;

    private static final float AP_PROXIMITY_SENSOR_FAR_DISTANCE_IN_CM = 5f;

    private ConcurrentLinkedQueue<ApSensorDatapoint> mApDatapointsQueue;
    private ConcurrentLinkedQueue<ChreSensorDatapoint> mChreDatapointsQueue;

    private ApSensorDatapoint[] mApDatapointsArray;
    private ChreSensorDatapoint[] mChreDatapointsArray;

    private SensorManager mSensorManager;
    private Sensor mSensor;

    private long mSamplingIntervalInMs;
    private boolean mChreSensorFound;

    private CrossValidatorSensorConfig mSensorConfig;

    private static final BiMap<Integer, Integer> AP_TO_CHRE_SENSOR_TYPE =
            makeApToChreSensorTypeMap();
    private static final Map<Integer, CrossValidatorSensorConfig> SENSOR_TYPE_TO_CONFIG =
            makeSensorTypeToInfoMap();

    /*
    * @param contextHubManager The context hub manager that will be passed to super ctor.
    * @param contextHubInfo The context hub info that will be passed to super ctor.
    * @param nappAppBinary The nanoapp binary that will be passed to super ctor.
    * @param apSensorType The sensor type that this sensor validator will validate against. This
    *     must be one of the int constants starting with TYPE_ defined in android.hardware.Sensor
    *     class.
    */
    public ChreCrossValidatorSensor(ContextHubManager contextHubManager,
            ContextHubInfo contextHubInfo, NanoAppBinary nanoAppBinary, int apSensorType)
            throws AssertionError {
        super(contextHubManager, contextHubInfo, nanoAppBinary);
        Assert.assertTrue("Nanoapp given to cross validator is not the designated chre cross"
                + " validation nanoapp.",
                nanoAppBinary.getNanoAppId() == NANO_APP_ID);
        mApDatapointsQueue = new ConcurrentLinkedQueue<ApSensorDatapoint>();
        mChreDatapointsQueue = new ConcurrentLinkedQueue<ChreSensorDatapoint>();
        Assert.assertTrue(String.format("Sensor type %d is not recognized", apSensorType),
                isSensorTypeValid(apSensorType));
        mSensorConfig = SENSOR_TYPE_TO_CONFIG.get(apSensorType);
        mSensorManager =
                (SensorManager) InstrumentationRegistry.getInstrumentation().getContext()
                .getSystemService(Context.SENSOR_SERVICE);
        Assert.assertNotNull("Sensor manager could not be instantiated.", mSensorManager);
        mSensor = mSensorManager.getDefaultSensor(apSensorType);
        Assume.assumeNotNull(String.format("Sensor could not be instantiated for sensor type %d.",
                apSensorType),
                mSensor);
        mSamplingIntervalInMs =
                Math.min(Math.max(
                        DEFAULT_SAMPLING_INTERVAL_IN_MS,
                        TimeUnit.MICROSECONDS.toMillis(mSensor.getMinDelay())),
                TimeUnit.MICROSECONDS.toMillis(mSensor.getMaxDelay()));
    }

    @Override
    public void validate() throws AssertionError {
        verifyChreSensorIsPresent();
        collectDataFromAp();
        collectDataFromChre();
        waitForDataSampling();
        assertApAndChreDataSimilar();
    }

    /**
    * @return The nanoapp message used to start the data collection in chre
    */
    private NanoAppMessage makeStartNanoAppMessage() {
        int messageType = ChreCrossValidationSensor.MessageType.CHRE_CROSS_VALIDATION_START_VALUE;
        ChreCrossValidationSensor.StartSensorCommand startSensor =
                ChreCrossValidationSensor.StartSensorCommand.newBuilder()
                .setChreSensorType(getChreSensorType())
                .setIntervalInMs(mSamplingIntervalInMs)
                .setLatencyInMs(SAMPLING_LATENCY_IN_MS)
                .setIsContinuous(sensorIsContinuous())
                .build();
        ChreCrossValidationSensor.StartCommand startCommand =
                ChreCrossValidationSensor.StartCommand.newBuilder()
                .setStartSensorCommand(startSensor).build();
        return NanoAppMessage.createMessageToNanoApp(
                mNappBinary.getNanoAppId(), messageType, startCommand.toByteArray());
    }

    /**
    * @return The nanoapp message used to retrieve info of a given CHRE sensor.
    */
    private NanoAppMessage makeInfoCommandMessage() {
        int messageType = ChreCrossValidationSensor.MessageType.CHRE_CROSS_VALIDATION_INFO_VALUE;
        ChreCrossValidationSensor.SensorInfoCommand infoCommand =
                ChreCrossValidationSensor.SensorInfoCommand.newBuilder()
                .setChreSensorType(getChreSensorType())
                .build();
        return NanoAppMessage.createMessageToNanoApp(
                mNappBinary.getNanoAppId(), messageType, infoCommand.toByteArray());
    }

    @Override
    protected void parseDataFromNanoAppMessage(NanoAppMessage message) {
        if (message.getMessageType()
                == ChreCrossValidationSensor.MessageType
                        .CHRE_CROSS_VALIDATION_INFO_RESPONSE_VALUE) {
            parseInfoResponseFromNanoappMessage(message);
        } else if (message.getMessageType()
                == ChreCrossValidationSensor.MessageType.CHRE_CROSS_VALIDATION_DATA_VALUE) {
            parseSensorDataFromNanoappMessage(message);
        } else {
            Assert.fail("Received invalid message type from nanoapp " + message.getMessageType());
        }
    }

    private void parseInfoResponseFromNanoappMessage(NanoAppMessage message) {
        ChreCrossValidationSensor.SensorInfoResponse infoProto;
        try {
            infoProto = ChreCrossValidationSensor.SensorInfoResponse.parseFrom(
                    message.getMessageBody());
        } catch (InvalidProtocolBufferException e) {
            setErrorStr("Error parsing protobuf: " + e);
            return;
        }
        if (!infoProto.hasChreSensorType() || !infoProto.hasIsAvailable()) {
            setErrorStr("Info response message isn't completely filled in");
            return;
        }

        int apSensorType = chreToApSensorType(infoProto.getChreSensorType());
        if (!isSensorTypeCurrent(apSensorType)) {
            setErrorStr(String.format("Incorrect sensor type %d when expecting %d",
                    apSensorType, mSensor.getType()));
            return;
        }

        mChreSensorFound = infoProto.getIsAvailable();
        mAwaitDataLatch.countDown();
    }

    private void parseSensorDataFromNanoappMessage(NanoAppMessage message) {
        final String kParseDataErrorPrefix = "While parsing data from nanoapp: ";
        ChreCrossValidationSensor.Data dataProto;
        try {
            dataProto = ChreCrossValidationSensor.Data.parseFrom(message.getMessageBody());
        } catch (InvalidProtocolBufferException e) {
            setErrorStr("Error parsing protobuff: " + e);
            return;
        }
        if (!dataProto.hasSensorData()) {
            setErrorStr(kParseDataErrorPrefix + "found non sensor type data");
        } else {
            ChreCrossValidationSensor.SensorData sensorData = dataProto.getSensorData();
            int sensorType = chreToApSensorType(sensorData.getChreSensorType());
            if (!isSensorTypeCurrent(sensorType)) {
                setErrorStr(
                        String.format(kParseDataErrorPrefix
                        + "incorrect sensor type %d when expecting %d",
                        sensorType, mSensor.getType()));
            } else {
                for (ChreCrossValidationSensor.SensorDatapoint datapoint :
                        sensorData.getDatapointsList()) {
                    int valuesLength = datapoint.getValuesList().size();
                    if (valuesLength != mSensorConfig.expectedValuesLength) {
                        setErrorStr(String.format(kParseDataErrorPrefix
                                + "incorrect sensor datapoints values length %d when expecing %d",
                                sensorType, valuesLength, mSensorConfig.expectedValuesLength));
                        break;
                    }
                    mChreDatapointsQueue.add(new ChreSensorDatapoint(datapoint));
                }
            }
        }
    }

    private void assertApAndChreDataSimilar() throws AssertionError {
        // Copy concurrent queues to arrays so that other threads will not mutate the data being
        // worked on
        mApDatapointsArray = mApDatapointsQueue.toArray(new ApSensorDatapoint[0]);
        mChreDatapointsArray = mChreDatapointsQueue.toArray(new ChreSensorDatapoint[0]);
        Assert.assertTrue("Did not find any CHRE datapoints", mChreDatapointsArray.length > 0);
        Assert.assertTrue("Did not find any AP datapoints", mApDatapointsArray.length > 0);
        alignApAndChreDatapoints();
        // AP and CHRE datapoints will be same size
        for (int i = 0; i < mApDatapointsArray.length; i++) {
            assertSensorDatapointsSimilar(
                    (ApSensorDatapoint) mApDatapointsArray[i],
                    (ChreSensorDatapoint) mChreDatapointsArray[i], i);
        }
    }

    private long getAwaitDataTimeoutInMs() {
        if (mSensor.getType() == Sensor.REPORTING_MODE_CONTINUOUS) {
            return AWAIT_DATA_TIMEOUT_CONTINUOUS_IN_MS;
        } else {
            return AWAIT_DATA_TIMEOUT_ON_CHANGE_ONE_SHOT_IN_MS;
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {}

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (mCollectingData.get()) {
            int sensorType = event.sensor.getType();
            if (!isSensorTypeCurrent(sensorType)) {
                setErrorStr(String.format("incorrect sensor type %d when expecting %d",
                                          sensorType, mSensor.getType()));
            } else {
                mApDatapointsQueue.add(new ApSensorDatapoint(event));
            }
        }
    }

    @Override
    public void init() throws AssertionError {
        super.init();
        restrictSensors();
    }

    @Override
    public void deinit() throws AssertionError {
        super.deinit();
        unrestrictSensors();
    }

    /*
    * @param sensorType The sensor type that was passed to the ctor that will be validated.
    * @return true if sensor type is recognized.
    */
    private static boolean isSensorTypeValid(int sensorType) {
        return SENSOR_TYPE_TO_CONFIG.containsKey(sensorType);
    }

    /**
     * @param sensorType The sensor type received from nanoapp or Android framework.
     * @return true if sensor type matches current sensor type expected.
     */
    private boolean isSensorTypeCurrent(int sensorType) {
        return sensorType == mSensor.getType();
    }

    /**
    * Make the sensor type info objects for each sensor type and map from sensor type to those
    * objects.
    *
    * @return The map from sensor type to info for that type.
    */
    private static Map<Integer, CrossValidatorSensorConfig> makeSensorTypeToInfoMap() {
        Map<Integer, CrossValidatorSensorConfig> map =
                new HashMap<Integer, CrossValidatorSensorConfig>();
        // new CrossValidatorSensorConfig(<expectedValuesLength>, <errorMargin>)
        map.put(Sensor.TYPE_ACCELEROMETER, new CrossValidatorSensorConfig(3, 0.01f));
        map.put(Sensor.TYPE_GYROSCOPE, new CrossValidatorSensorConfig(3, 0.01f));
        map.put(Sensor.TYPE_MAGNETIC_FIELD, new CrossValidatorSensorConfig(3, 0.05f));
        map.put(Sensor.TYPE_PRESSURE, new CrossValidatorSensorConfig(1, 0.01f));
        map.put(Sensor.TYPE_LIGHT, new CrossValidatorSensorConfig(1, 0.07f));
        map.put(Sensor.TYPE_PROXIMITY, new CrossValidatorSensorConfig(1, 0.01f));
        return map;
    }

    /**
    * Make the map from CHRE sensor type values to their AP values.
    *
    * @return The map from sensor type to info for that type.
    */
    private static BiMap<Integer, Integer> makeApToChreSensorTypeMap() {
        BiMap<Integer, Integer> map = HashBiMap.create(4);
        // CHRE sensor type constants in //system/chre/chre_api/include/chre_api/chre/sensor_types.h
        map.put(Sensor.TYPE_ACCELEROMETER, 1 /* CHRE_SENSOR_TYPE_ACCELEROMETER */);
        map.put(Sensor.TYPE_GYROSCOPE, 6 /* CHRE_SENSOR_TYPE_GYROSCOPE */);
        map.put(Sensor.TYPE_MAGNETIC_FIELD, 8 /* CHRE_SENSOR_TYPE_MAGNETIC_FIELD */);
        map.put(Sensor.TYPE_PRESSURE, 10 /* CHRE_SENSOR_TYPE_PRESSURE */);
        map.put(Sensor.TYPE_LIGHT, 12 /* CHRE_SENSOR_TYPE_LIGHT */);
        map.put(Sensor.TYPE_PROXIMITY, 13 /* CHRE_SENSOR_TYPE_PROXIMITY */);
        return map;
    }

    /**
    * Start collecting data from AP
    */
    private void collectDataFromAp() {
        Assert.assertTrue(mSensorManager.registerListener(
                this, mSensor, (int) TimeUnit.MILLISECONDS.toMicros(mSamplingIntervalInMs)));
    }

    /**
    * Start collecting data from CHRE
    */
    private void collectDataFromChre() {
        // The info in the start message will inform the nanoapp of which type of
        // data to collect (accel, gyro, gnss, wifi, etc).
        sendMessageToNanoApp(makeStartNanoAppMessage());
    }

    private void sendMessageToNanoApp(NanoAppMessage message) {
        int result = mContextHubClient.sendMessageToNanoApp(message);
        if (result != ContextHubTransaction.RESULT_SUCCESS) {
            Assert.fail("Collect data from CHRE failed with result "
                    + contextHubTransactionResultToString(result)
                    + " while trying to send start message.");
        }
    }

    /**
    * Wait for AP and CHRE data to be fully collected or timeouts occur. collectDataFromAp and
    * collectDataFromChre methods should both be called before this.
    *
    * @param samplingDurationInMs The amount of time to wait for AP and CHRE to collected data in
    * ms.
    */
    private void waitForDataSampling() throws AssertionError {
        mCollectingData.set(true);
        try {
            mAwaitDataLatch.await(getAwaitDataTimeoutInMs(), TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Assert.fail("await data latch interrupted");
        }
        if (mErrorStr.get() != null) {
            Assert.fail(mErrorStr.get());
        }
        mCollectingData.set(false);
    }

    /*
    * Align the AP and CHRE datapoints by finding all the timestamps that match up and discarding
    * the rest of the datapoints.
    */
    private void alignApAndChreDatapoints() throws AssertionError {
        ArrayList<ApSensorDatapoint> newApSensorDatapoints = new ArrayList<ApSensorDatapoint>();
        ArrayList<ChreSensorDatapoint> newChreSensorDatapoints =
                new ArrayList<ChreSensorDatapoint>();
        int apI = 0;
        int chreI = 0;
        while (apI < mApDatapointsArray.length && chreI < mChreDatapointsArray.length) {
            ApSensorDatapoint apDp = mApDatapointsArray[apI];
            ChreSensorDatapoint chreDp = mChreDatapointsArray[chreI];
            if (datapointTimestampsAreSimilar(apDp, chreDp)) {
                newApSensorDatapoints.add(apDp);
                newChreSensorDatapoints.add(chreDp);
                apI++;
                chreI++;
            } else if (apDp.timestamp < chreDp.timestamp) {
                apI++;
            } else {
                chreI++;
            }
        }
        // TODO(b/175795665): Assert that an acceptable amount of datapoints pass the alignment
        // phase.
        Assert.assertTrue("Did not find matching timestamps to align AP and CHRE datapoints.",
                          !(newApSensorDatapoints.isEmpty() || newChreSensorDatapoints.isEmpty()));
        mApDatapointsArray = newApSensorDatapoints.toArray(new ApSensorDatapoint[0]);
        mChreDatapointsArray = newChreSensorDatapoints.toArray(new ChreSensorDatapoint[0]);
    }

    /**
    * Restrict other applications from accessing sensors. Should be called before validating data.
    */
    private void restrictSensors() {
        ChreTestUtil.executeShellCommand(InstrumentationRegistry.getInstrumentation(),
                "dumpsys sensorservice restrict ChreCrossValidatorSensor");
    }

    /**
    * Unrestrict other applications from accessing sensors. Should be called after validating data.
    */
    private void unrestrictSensors() {
        ChreTestUtil.executeShellCommand(
                InstrumentationRegistry.getInstrumentation(), "dumpsys sensorservice enable");
    }

    @Override
    protected void unregisterApDataListener() {
        mSensorManager.unregisterListener(this);
    }

    /**
     * Helper method for asserting a single pair of AP and CHRE datapoints are similar.
     */
    private void assertSensorDatapointsSimilar(ApSensorDatapoint apDp,
                                               ChreSensorDatapoint chreDp, int index) {
        String datapointsAssertMsg =
                String.format("AP and CHRE three axis datapoint values differ on index %d", index)
                + "\nAP data -> " + apDp + "\nCHRE data -> "
                + chreDp;

        // TODO(b/146052784): Log full list of datapoints to file on disk on assertion failure
        // so that there is more insight into the problem then just logging the one pair of
        // datapoints
        Assert.assertTrue(datapointsAssertMsg,
                datapointValuesAreSimilar(
                apDp, chreDp, mSensorConfig.errorMargin));
    }

    /**
     * @param chreSensorType The CHRE sensor type value.
     *
     * @return The AP sensor type value.
     */
    private static int chreToApSensorType(int chreSensorType) {
        return AP_TO_CHRE_SENSOR_TYPE.inverse().get(chreSensorType);
    }

    /**
     * @return The CHRE sensor type of the sensor being validated.
     */
    private int getChreSensorType() {
        return AP_TO_CHRE_SENSOR_TYPE.get(mSensor.getType());
    }

    /**
     * Verify the CHRE sensor being evaluated is present on this device.
     */
    private void verifyChreSensorIsPresent() {
        mCollectingData.set(true);
        sendMessageToNanoApp(makeInfoCommandMessage());
        waitForInfoResponse();
        mCollectingData.set(false);
        // All CHRE sensors are optional so skip this test if the required sensor isn't found.
        Assume.assumeTrue(mChreSensorFound);
    }

    private void waitForInfoResponse() {
        boolean success = false;
        try {
            success = mAwaitDataLatch.await(INFO_RESPONSE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Assert.fail("await data latch interrupted");
        }

        if (!success) {
            Assert.fail("Timed out waiting for sensor info response");
        }

        if (mErrorStr.get() != null) {
            Assert.fail(mErrorStr.get());
        }

        // Reset latch for use in waiting for sensor data.
        mAwaitDataLatch = new CountDownLatch(1);
    }

    private boolean sensorIsContinuous() {
        return mSensor.getReportingMode() == Sensor.REPORTING_MODE_CONTINUOUS;
    }

    /*
     * @param apDp The AP sensor datapoint object to compare.
     * @param chreDp The CHRE sensor datapoint object to compare.
     *
     * @return true if timestamps are similar.
     */
    private static boolean datapointTimestampsAreSimilar(SensorDatapoint apDp,
                                                         SensorDatapoint chreDp) {
        return Math.abs(apDp.timestamp - chreDp.timestamp) < MAX_TIMESTAMP_DIFF_NS;
    }

    /*
     * @param apDp The AP SensorDatapoint object to compare.
     * @param chreDp The CHRE SensorDatapoint object to compare.
     * @param errorMargin The amount that each value in values array can differ between the two
     *     datapoints.
     * @return true if the datapoint values are all similar.
     */
    private static boolean datapointValuesAreSimilar(
            ApSensorDatapoint apDp, ChreSensorDatapoint chreDp, float errorMargin) {
        Assert.assertEquals(apDp.values.length, chreDp.values.length);
        for (int i = 0; i < apDp.values.length; i++) {
            if (apDp.sensor.getType() == Sensor.TYPE_PROXIMITY) {
                // CHRE proximity values are 0 if near and otherwise are far.
                boolean chreIsNear = chreDp.values[i] == 0f;
                // AP proximity values are near if they are less than a constant distance defined in
                // AP_PROXIMITY_SENSOR_FAR_DISTANCE_IN_CM or less than max value if their max value
                // is less than this constant and far if it exceeds this constant or is set to the
                // max value.
                boolean apIsNear = apDp.values[i] < Math.min(
                        apDp.sensor.getMaximumRange(), AP_PROXIMITY_SENSOR_FAR_DISTANCE_IN_CM);
                if (chreIsNear != apIsNear) {
                    return false;
                }
            } else {
                float diff = Math.abs(apDp.values[i] - chreDp.values[i]);
                // TODO(b/157732778): Find a better way to compare sensor values.
                if (diff > errorMargin) {
                    return false;
                }
            }
        }
        return true;
    }
}
