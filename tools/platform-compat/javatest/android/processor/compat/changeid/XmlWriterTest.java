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

package android.processor.compat.changeid;

import static org.hamcrest.core.StringStartsWith.startsWith;
import static org.junit.Assert.assertThat;

import org.junit.Test;

import java.io.ByteArrayOutputStream;
import java.io.OutputStream;

public class XmlWriterTest {

    private static final String HEADER =
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>";

    private OutputStream mOutputStream = new ByteArrayOutputStream();

    @Test
    public void testNoChanges() throws Exception {
        XmlWriter writer = new XmlWriter();
        writer.write(mOutputStream);

        String expected = HEADER + "<config/>";

        assertThat(mOutputStream.toString(), startsWith(expected));
    }

    @Test
    public void testOneChange() throws Exception {
        XmlWriter writer = new XmlWriter();
        Change c = new Change.Builder()
                .id(123456789L)
                .name("change-name")
                .build();

        writer.addChange(c);
        writer.write(mOutputStream);

        String expected = HEADER + "<config>"
                + "<compat-change id=\"123456789\" name=\"change-name\"/>"
                + "</config>";

        assertThat(mOutputStream.toString(), startsWith(expected));
    }

    @Test
    public void testSomeChanges() throws Exception {
        XmlWriter writer = new XmlWriter();
        Change c = new Change.Builder()
                .id(111L)
                .name("change-name1")
                .description("my nice change")
                .build();
        Change disabled = new Change.Builder()
                .id(222L)
                .name("change-name2")
                .disabled()
                .build();
        Change sdkRestricted = new Change.Builder()
                .id(333L)
                .name("change-name3")
                .enabledAfter(28)
                .build();
        Change both = new Change.Builder()
                .id(444L)
                .name("change-name4")
                .disabled()
                .enabledAfter(29)
                .build();
        Change loggingOnly = new Change.Builder()
                .id(555L)
                .name("change-name5")
                .loggingOnly()
                .build();

        writer.addChange(c);
        writer.addChange(disabled);
        writer.addChange(sdkRestricted);
        writer.addChange(both);
        writer.addChange(loggingOnly);
        writer.write(mOutputStream);

        String expected = HEADER + "<config>"
                + "<compat-change description=\"my nice change\" id=\"111\" name=\"change-name1\"/>"
                + "<compat-change disabled=\"true\" id=\"222\" name=\"change-name2\"/>"
                + "<compat-change enableAfterTargetSdk=\"28\" id=\"333\" "
                + "name=\"change-name3\"/>"
                + "<compat-change disabled=\"true\" enableAfterTargetSdk=\"29\" id=\"444\" "
                + "name=\"change-name4\"/>"
                + "<compat-change id=\"555\" loggingOnly=\"true\" name=\"change-name5\"/>"
                + "</config>";

        assertThat(mOutputStream.toString(), startsWith(expected));
    }
}
