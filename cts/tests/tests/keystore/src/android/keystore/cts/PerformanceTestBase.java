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
 * limitations under the License
 */

package android.keystore.cts;

import android.os.SystemClock;
import android.security.keystore.KeyGenParameterSpec;
import android.test.AndroidTestCase;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.DeviceReportLog;
import com.android.compatibility.common.util.ResultType;
import com.android.compatibility.common.util.ResultUnit;

import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.KeyStore;
import java.security.cert.Certificate;
import java.util.ArrayList;

import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;

import android.os.Build;

public class PerformanceTestBase extends AndroidTestCase {

    public static final long MS_PER_NS = 1000000L;
    protected static final String TAG = "KeystorePerformanceTest";
    private static final String REPORT_LOG_NAME = "CtsKeystoreTestCases";
    /**
     * Number of milliseconds to spend repeating a single test.
     *
     * <p>For each algorithm we run the test repeatedly up to a maximum of this time limit (or up to
     * {@link #TEST_ITERATION_LIMIT} whichever is reached first), then report back the number of
     * repetitions. We don't abort a test at the time limit but let it run to completion, so we're
     * guaranteed to always get at least one repetition, even if it takes longer than the limit.
     */
    private static int TEST_TIME_LIMIT = 20000;

    /** Maximum number of iterations to run a single test. */
    static int TEST_ITERATION_LIMIT = 20;

    protected void measure(Measurable... measurables) throws Exception {
        ArrayList<PerformanceTestResult> results = new ArrayList<>();
        results.ensureCapacity(measurables.length);
        for (Measurable measurable : measurables) {
            DeviceReportLog reportLog = new DeviceReportLog(REPORT_LOG_NAME, "performance_test");
            PerformanceTestResult result = measure(measurable);

            reportLog.addValue(
                    "test_environment",
                    measurable.getEnvironment(),
                    ResultType.NEUTRAL,
                    ResultUnit.NONE);
            reportLog.addValue(
                    "test_name", measurable.getName(), ResultType.NEUTRAL, ResultUnit.NONE);
            reportLog.addValue(
                    "sample_count", result.getSampleCount(), ResultType.NEUTRAL, ResultUnit.COUNT);
            reportLog.addValue(
                    "setup_time", result.getSetupTime(), ResultType.LOWER_BETTER, ResultUnit.MS);
            reportLog.addValue(
                    "mean_time", result.getMean(), ResultType.LOWER_BETTER, ResultUnit.MS);
            reportLog.addValue(
                    "sample_std_dev",
                    result.getSampleStdDev(),
                    ResultType.LOWER_BETTER,
                    ResultUnit.MS);
            reportLog.addValue(
                    "median_time", result.getMedian(), ResultType.LOWER_BETTER, ResultUnit.MS);
            reportLog.addValue(
                    "percentile_90_time",
                    result.getPercentile(0.9),
                    ResultType.LOWER_BETTER,
                    ResultUnit.MS);
            reportLog.addValue(
                    "teardown_time",
                    result.getTearDownTime(),
                    ResultType.LOWER_BETTER,
                    ResultUnit.MS);
            reportLog.addValue(
                    "teardown_time",
                    result.getTearDownTime(),
                    ResultType.LOWER_BETTER,
                    ResultUnit.MS);
            reportLog.submit(InstrumentationRegistry.getInstrumentation());
        }
    }

    private PerformanceTestResult measure(Measurable measurable) throws Exception {
        // One un-measured time through everything, to warm caches, etc.

        PerformanceTestResult result = new PerformanceTestResult();

        measurable.initialSetUp();
        measurable.setUp();
        measurable.measure();
        measurable.tearDown();

        long runLimit = now() + TEST_TIME_LIMIT * MS_PER_NS;
        while (now() < runLimit && result.getSampleCount() < TEST_ITERATION_LIMIT) {
            long setupBegin = now();
            measurable.setUp();
            result.addSetupTime(now() - setupBegin);

            long runBegin = now();
            measurable.measure();
            result.addMeasurement(now() - runBegin);

            long tearDownBegin = now();
            measurable.tearDown();
            result.addTeardownTime(now() - tearDownBegin);
        }
        measurable.finalTearDown();

        return result;
    }

    protected long now() {
        return SystemClock.elapsedRealtimeNanos();
    }

    public abstract class Measurable {

        private Measurable() {}
        ;

        public abstract String getEnvironment();

        public abstract String getName();

        public void initialSetUp() throws Exception {}

        public void setUp() throws Exception {}

        public abstract void measure() throws Exception;

        public void tearDown() throws Exception {}

        public void finalTearDown() throws Exception {}
    }

    /** Base class for measuring Keystore operations. */
    abstract class KeystoreMeasurable extends Measurable {
        private final String mName;
        private final byte[] mMessage;
        private final KeystoreKeyGenerator mGenerator;

        KeystoreMeasurable(
                KeystoreKeyGenerator generator, String operation, int keySize, int messageSize)
                throws Exception {
            super();
            mGenerator = generator;
            if (messageSize < 0) {
                mName = (operation
                                + "/" + getAlgorithm()
                                + "/" + keySize);
                mMessage = null;
            } else {
                mName = (operation
                                + "/" + getAlgorithm()
                                + "/" + keySize
                                + "/" + messageSize);
                mMessage = TestUtils.generateRandomMessage(messageSize);
            }
        }

        KeystoreMeasurable(KeystoreKeyGenerator generator, String operation, int keySize)
                throws Exception {
            this(generator, operation, keySize, -1);
        }

        @Override
        public String getEnvironment() {
            return mGenerator.getProvider() + "/" + Build.CPU_ABI;
        }

        @Override
        public String getName() {
            return mName;
        }

        byte[] getMessage() {
            return mMessage;
        }

        String getAlgorithm() {
            return mGenerator.getAlgorithm();
        }

        @Override
        public void finalTearDown() throws Exception {
            deleteKey();
        }

        public void deleteKey() throws Exception {
            mGenerator.deleteKey();
        }

        SecretKey generateSecretKey() throws Exception {
            return mGenerator.getSecretKeyGenerator().generateKey();
        }

        KeyPair generateKeyPair() throws Exception {
            return mGenerator.getKeyPairGenerator().generateKeyPair();
        }
    }

    /**
     * Measurable for generating key pairs.
     *
     * <p>This class is ignostic to the Keystore provider or key algorithm.
     */
    class KeystoreKeyPairGenMeasurable extends KeystoreMeasurable {

        KeystoreKeyPairGenMeasurable(KeystoreKeyGenerator keyGenerator, int keySize)
                throws Exception {
            super(keyGenerator, "keygen", keySize);
        }

        @Override
        public void measure() throws Exception {
            generateKeyPair();
        }

        @Override
        public void tearDown() throws Exception {
            deleteKey();
        }
    }

    /**
     * Measurable for generating a secret key.
     *
     * <p>This class is ignostic to the Keystore provider or key algorithm.
     */
    class KeystoreSecretKeyGenMeasurable extends KeystoreMeasurable {

        KeystoreSecretKeyGenMeasurable(KeystoreKeyGenerator keyGenerator, int keySize)
                throws Exception {
            super(keyGenerator, "keygen", keySize);
        }

        @Override
        public void measure() throws Exception {
            generateSecretKey();
        }

        @Override
        public void tearDown() throws Exception {
            deleteKey();
        }
    }

    /**
     * Wrapper for generating Keystore keys.
     *
     * <p>Abstracts the provider and initilization of the generator.
     */
    abstract class KeystoreKeyGenerator {
        private final String mAlgorithm;
        private final String mProvider;
        private KeyGenerator mSecretKeyGenerator = null;
        private KeyPairGenerator mKeyPairGenerator = null;

        KeystoreKeyGenerator(String algorithm, String provider) throws Exception {
            mAlgorithm = algorithm;
            mProvider = provider;
        }

        KeystoreKeyGenerator(String algorithm) throws Exception {
            // This is a hack to get the default provider.
            this(algorithm, KeyGenerator.getInstance("AES").getProvider().getName());
        }

        String getAlgorithm() {
            return mAlgorithm;
        }

        String getProvider() {
            return mProvider;
        }

        /** By default, deleteKey is a nop */
        void deleteKey() throws Exception {}

        KeyGenerator getSecretKeyGenerator() throws Exception {
            if (mSecretKeyGenerator == null) {
                mSecretKeyGenerator =
                        KeyGenerator.getInstance(TestUtils.getKeyAlgorithm(mAlgorithm), mProvider);
            }
            return mSecretKeyGenerator;
        }

        KeyPairGenerator getKeyPairGenerator() throws Exception {
            if (mKeyPairGenerator == null) {
                mKeyPairGenerator =
                        KeyPairGenerator.getInstance(
                                TestUtils.getKeyAlgorithm(mAlgorithm), mProvider);
            }
            return mKeyPairGenerator;
        }
    }

    /**
     * Wrapper for generating Android Keystore keys.
     *
     * <p>Provides Android Keystore specific functionality, like deleting the key.
     */
    abstract class AndroidKeystoreKeyGenerator extends KeystoreKeyGenerator {
        private final KeyStore mKeyStore;
        private final String KEY_ALIAS = "perf_key";

        AndroidKeystoreKeyGenerator(String algorithm) throws Exception {
            super(algorithm, TestUtils.EXPECTED_PROVIDER_NAME);
            mKeyStore = KeyStore.getInstance(getProvider());
            mKeyStore.load(null);
        }

        @Override
        void deleteKey() throws Exception {
            mKeyStore.deleteEntry(KEY_ALIAS);
        }

        KeyGenParameterSpec.Builder getKeyGenParameterSpecBuilder(int purpose) {
            return new KeyGenParameterSpec.Builder(KEY_ALIAS, purpose);
        }

        Certificate[] getCertificateChain() throws Exception {
            return mKeyStore.getCertificateChain(KEY_ALIAS);
        }
    }

    /** Basic generator for KeyPairs that uses the default provider. */
    class DefaultKeystoreKeyPairGenerator extends KeystoreKeyGenerator {
        DefaultKeystoreKeyPairGenerator(String algorithm, int keySize) throws Exception {
            super(algorithm);
            getKeyPairGenerator().initialize(keySize);
        }
    }

    /** Basic generator for SecretKeys that uses the default provider. */
    class DefaultKeystoreSecretKeyGenerator extends KeystoreKeyGenerator {
        DefaultKeystoreSecretKeyGenerator(String algorithm, int keySize) throws Exception {
            super(algorithm);
            getSecretKeyGenerator().init(keySize);
        }
    }
}
