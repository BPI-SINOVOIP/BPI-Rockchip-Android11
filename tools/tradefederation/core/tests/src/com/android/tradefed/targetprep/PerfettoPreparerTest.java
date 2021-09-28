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
package com.android.tradefed.targetprep;

import static org.junit.Assert.fail;

import com.android.tradefed.config.OptionSetter;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.util.FileUtil;

import org.easymock.EasyMock;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.io.File;
import java.io.IOException;

/** Unit tests for {@link PerfettoPreparer} */
@RunWith(JUnit4.class)
public class PerfettoPreparerTest {

    private static final String DEVICE_CONFIG_PATH = "/data/misc/perfetto-traces/trace_config.pb";

    private PerfettoPreparer mPreparer = null;
    private ITestDevice mMockDevice = null;
    private OptionSetter mOptionSetter = null;

    @Before
    public void setUp() throws Exception {
        mMockDevice = EasyMock.createStrictMock(ITestDevice.class);
        EasyMock.expect(mMockDevice.getDeviceDescriptor()).andStubReturn(null);
        EasyMock.expect(mMockDevice.getSerialNumber()).andStubReturn("SERIAL");
        mPreparer = new PerfettoPreparer();
        mOptionSetter = new OptionSetter(mPreparer);
    }

    /** When there's nothing to be done, expect no exception to be thrown */
    @Test
    public void testNoop() throws Exception {
        EasyMock.replay(mMockDevice);
        mPreparer.setUp(mMockDevice, null);
        EasyMock.verify(mMockDevice);
    }

    /** Test if exception is thrown if the local perfetto binary file doen't exist */
    @Test
    public void testLocalPerfettoBinaryNoExist() throws Exception {
        mOptionSetter.setOptionValue("binary-perfetto-config", "dummy.txt");
        EasyMock.replay(mMockDevice);
        try {
            mPreparer.setUp(mMockDevice, null);
            fail("TargetSetupError not thrown");
        } catch (TargetSetupError e) {
            // expected
        }
        EasyMock.verify(mMockDevice);
    }

    /** Test no exception is thrown if the local binary perfetto file is passed*/
    @Test
    public void testLocalPerfettoBinaryValid() throws Exception {
        // Doesn't have to be binary file for testing purpose.
        File perfettoTextFile = createPerfettoFile(true);
        mOptionSetter.setOptionValue("binary-perfetto-config", perfettoTextFile.getPath());
        EasyMock.expect(
                mMockDevice.pushFile(
                        (File) EasyMock.anyObject(), EasyMock.eq(DEVICE_CONFIG_PATH)))
                .andReturn(Boolean.TRUE);
        EasyMock.replay(mMockDevice);
        try {
            // Should not throw any exception.
            mPreparer.setUp(mMockDevice, null);
        } finally {
            perfettoTextFile.delete();
        }
        EasyMock.verify(mMockDevice);
    }

    /**
     * Creates perfetto valid or invalid config text file.
     *
     * @param valid if true create valid perfetto config file.
     * @return the created file
     */
    private File createPerfettoFile(boolean valid) throws IOException {
        File tempFile = File.createTempFile("textproto-perfetto-config", ".txt");
        if (valid) {
            FileUtil.writeToFile(
                    "data_sources {\n"
                            + "config {\n"
                            + "name: \"linux.process_stats\"\n"
                            + "target_buffer: 0\n"
                            + "}\n"
                            + "}\n",
                    tempFile);
        } else {
            FileUtil.writeToFile("xyz", tempFile);
        }
        return tempFile;
    }
}
