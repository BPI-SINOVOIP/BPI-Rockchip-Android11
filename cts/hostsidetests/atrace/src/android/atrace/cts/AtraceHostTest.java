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

package android.atrace.cts;

import static android.atrace.cts.AtraceDeviceTestList.assertTracingOff;
import static android.atrace.cts.AtraceDeviceTestList.assertTracingOn;
import static android.atrace.cts.AtraceDeviceTestList.asyncBeginEndSection;
import static android.atrace.cts.AtraceDeviceTestList.beginEndSection;
import static android.atrace.cts.AtraceDeviceTestList.counter;
import static android.atrace.cts.AtraceDeviceTestList.launchActivity;

import com.android.tradefed.log.LogUtil.CLog;

import java.io.BufferedReader;
import java.io.Reader;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import kotlin.Unit;
import trebuchet.model.Counter;
import trebuchet.model.CounterValue;
import trebuchet.model.Model;
import trebuchet.model.ProcessModel;
import trebuchet.model.ThreadModel;
import trebuchet.model.base.Slice;
import trebuchet.model.fragments.AsyncSlice;
import trebuchet.queries.slices.*;

/**
 * Test to check that atrace is usable, to enable usage of systrace.
 */
public class AtraceHostTest extends AtraceHostTestBase {

    private interface FtraceEntryCallback {
        void onTraceEntry(String threadName, int pid, int tid, String eventType, String args);
        void onFinished();
    }

    /**
     * Helper for parsing ftrace data.
     * Regexs copied from (and should be kept in sync with) ftrace importer in catapult.
     */
    private static class FtraceParser {
        private static final Pattern sFtraceLine = Pattern.compile(
                "^ *(.{1,16})-(\\d+) +(?:\\( *(\\d+)?-*\\) )?\\[(\\d+)] (?:[dX.]...)? *([\\d.]*): *"
                + "([^:]*): *(.*) *$");

        private static void parseLine(String line, FtraceEntryCallback callback) {
            Matcher m = sFtraceLine.matcher(line);
            if (m.matches()) {
                callback.onTraceEntry(
                        /*threadname*/ m.group(1),
                        /*pid*/ (m.group(3) == null || m.group(3).startsWith("-")) ? -1 : Integer.parseInt(m.group(3)),
                        /*tid*/ Integer.parseInt(m.group(2)),
                        /*eventName*/ m.group(6),
                        /*details*/ m.group(7));
                return;
            }

            CLog.i("line doesn't match: " + line);
        }

        private static void parse(Reader reader, FtraceEntryCallback callback) throws Exception {
            try {
                BufferedReader bufferedReader = new BufferedReader(reader);
                String line;
                while ((line = bufferedReader.readLine()) != null) {
                    FtraceParser.parseLine(line, callback);
                }
            } finally {
                callback.onFinished();
            }
        }
    }

    /**
     * Tests that atrace exists and is runnable with no args
     */
    public void testSimpleRun() {
        String output = shell("atrace");
        String[] lines = output.split("\\r?\\n");

        // check for expected stdout
        assertEquals("capturing trace... done", lines[0]);
        assertEquals("TRACE:", lines[1]);

        // commented trace marker starts here
        assertEquals("# tracer: nop", lines[2]);
    }

    /**
     * Tests the output of "atrace --list_categories" to ensure required categories exist.
     */
    public void testCategories() {
        String output = shell("atrace --list_categories");
        String[] categories = output.split("\\r?\\n");

        Set<String> requiredCategories = new HashSet<String>(Arrays.asList(
                AtraceConfig.RequiredCategories));

        for (String category : categories) {
            int dashIndex = category.indexOf("-");

            assertTrue(dashIndex > 1); // must match category output format
            category = category.substring(0, dashIndex).trim();

            requiredCategories.remove(category);
        }

        if (!requiredCategories.isEmpty()) {
            for (String missingCategory : requiredCategories) {
                System.err.println("missing category: " + missingCategory);
            }
            fail("Expected categories missing from atrace");
        }
    }

    public void testTracingIsEnabled() {
        runSingleAppTest(assertTracingOff);
        traceSingleTest(assertTracingOn, true);
        runSingleAppTest(assertTracingOff);
    }

    // Verifies that although tracing is active, Trace.isEnabled() is false since the app
    // category isn't enabled
    public void testTracingIsDisabled() {
        runSingleAppTest(assertTracingOff);
        traceSingleTest(assertTracingOff, false);
        runSingleAppTest(assertTracingOff);
    }

    private static ThreadModel findThread(Model model, int id) {
        for (ProcessModel process : model.getProcesses().values()) {
            for (ThreadModel thread : process.getThreads()) {
                if (thread.getId() == id) {
                    return thread;
                }
            }
        }
        return null;
    }

    private static ProcessModel findProcess(Model model, int id) {
        for (ProcessModel process : model.getProcesses().values()) {
            if (process.getId() == id) {
                return process;
            }
        }
        return null;
    }

    private static Counter findCounter(ProcessModel processModel, String name) {
        for (Counter counter : processModel.getCounters()) {
            if (name.equals(counter.getName())) {
                return counter;
            }
        }
        return null;
    }

    public void testBeginEndSection() {
        TraceResult result = traceSingleTest(beginEndSection);
        assertTrue(result.getPid() > 0);
        assertTrue(result.getTid() > 0);
        assertNotNull(result.getModel());
        ThreadModel thread = findThread(result.getModel(), result.getTid());
        assertNotNull(thread);
        assertEquals(2, thread.getSlices().size());
        Slice sdkSlice = thread.getSlices().get(0);
        assertEquals("AtraceDeviceTest::beginEndSection", sdkSlice.getName());
        Slice ndkSlice = thread.getSlices().get(1);
        assertEquals("ndk::beginEndSection", ndkSlice.getName());
    }

    public void testAsyncBeginEndSection() {
        TraceResult result = traceSingleTest(asyncBeginEndSection);
        assertTrue(result.getPid() > 0);
        assertTrue(result.getTid() > 0);
        assertNotNull(result.getModel());
        ProcessModel process = findProcess(result.getModel(), result.getPid());
        assertNotNull(process);
        assertEquals(2, process.getAsyncSlices().size());
        AsyncSlice sdkSlice = process.getAsyncSlices().get(0);
        assertEquals("AtraceDeviceTest::asyncBeginEndSection", sdkSlice.getName());
        assertEquals(42, sdkSlice.getCookie());
        AsyncSlice ndkSlice = process.getAsyncSlices().get(1);
        assertEquals("ndk::asyncBeginEndSection", ndkSlice.getName());
        assertEquals(4770, ndkSlice.getCookie());
    }

    public void testCounter() {
        TraceResult result = traceSingleTest(counter);
        assertTrue(result.getPid() > 0);
        assertTrue(result.getTid() > 0);
        assertNotNull(result.getModel());
        ProcessModel process = findProcess(result.getModel(), result.getPid());
        assertNotNull(process);
        assertTrue(process.getCounters().size() > 0);
        Counter counter = findCounter(process, "AtraceDeviceTest::counter");
        assertNotNull(counter);
        List<CounterValue> values = counter.getEvents();
        assertEquals(4, values.size());
        assertEquals(10, values.get(0).getCount());
        assertEquals(20, values.get(1).getCount());
        assertEquals(30, values.get(2).getCount());
        assertEquals(9223372000000005807L, values.get(3).getCount());
    }

    /**
     * Tests that atrace captures app launch, including app level tracing
     */
    public void testTracingContent() {
        turnScreenOn();
        TraceResult result = traceSingleTest(launchActivity, AtraceConfig.DefaultCategories);

        final Set<String> requiredSections = new HashSet<>(Arrays.asList(
                // From the 'view' category
                "inflate",
                "Choreographer#doFrame",
                "traversal",
                "measure",
                "layout",
                "draw",
                "Record View#draw()",

                // From our app code
                "MyView::onDraw"
        ));

        ThreadModel thread = findThread(result.getModel(), result.getPid());
        SliceQueriesKt.iterSlices(thread, (Slice slice) -> {
            requiredSections.remove(slice.getName());
            return Unit.INSTANCE;
        });

        assertEquals("Didn't find all required sections",
                0, requiredSections.size());

        ProcessModel processModel = findProcess(result.getModel(), result.getPid());
        Counter drawCounter = findCounter(processModel, "MyView::drawCount");
        assertNotNull(drawCounter);
        assertTrue(drawCounter.getEvents().size() > 0);
        long previousCount = 0;
        for (CounterValue value : drawCounter.getEvents()) {
            assertTrue(previousCount < value.getCount());
            previousCount = value.getCount();
        }
        assertTrue(previousCount > 0);

        final Set<String> requiredAsyncSections = new HashSet<>(Arrays.asList(
                "AtraceActivity::created",
                "AtraceActivity::started",
                "AtraceActivity::resumed"
        ));

        for (AsyncSlice asyncSlice : processModel.getAsyncSlices()) {
            if (requiredAsyncSections.remove(asyncSlice.getName())) {
                assertEquals(1, asyncSlice.getCookie());
            }
        }
        assertEquals("Didn't find all async sections",
                0, requiredAsyncSections.size());
    }
}
