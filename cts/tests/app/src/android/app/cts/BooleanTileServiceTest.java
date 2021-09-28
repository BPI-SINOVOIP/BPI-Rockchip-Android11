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

package android.app.cts;


import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.stubs.ToggleableTestTileService;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;

import org.junit.Test;

public class BooleanTileServiceTest extends BaseTileServiceTest {
    private final static String TAG = "BooleanTileServiceTest";

    @Test
    public void testTileIsBoundAndListening() throws Exception {
        startTileService();
        expandSettings(true);
        waitForListening(true);
    }

    @Test
    public void testTileInDumpAndHasBooleanState() throws Exception {
        initializeAndListen();

        final CharSequence tileLabel = mTileService.getQsTile().getLabel();

        final String[] dumpLines = executeShellCommand(DUMP_COMMAND).split("\n");
        final String line = findLine(dumpLines, tileLabel);
        assertNotNull(line);
        assertTrue(line.trim().startsWith("BooleanState"));
    }

    @Test
    public void testTileStartsInactive() throws Exception {
        initializeAndListen();

        assertEquals(Tile.STATE_INACTIVE, mTileService.getQsTile().getState());
    }

    @Test
    public void testValueTracksState() throws Exception {
        initializeAndListen();

        final CharSequence tileLabel = mTileService.getQsTile().getLabel();

        String[] dumpLines = executeShellCommand(DUMP_COMMAND).split("\n");
        String line = findLine(dumpLines, tileLabel);

        // Tile starts inactive
        assertTrue(line.contains("value=false"));

        ((ToggleableTestTileService) mTileService).toggleState();

        // Close and open QS to make sure that state is refreshed
        expandSettings(false);
        waitForListening(false);
        expandSettings(true);
        waitForListening(true);

        assertEquals(Tile.STATE_ACTIVE, mTileService.getQsTile().getState());

        dumpLines = executeShellCommand(DUMP_COMMAND).split("\n");
        line = findLine(dumpLines, tileLabel);

        assertTrue(line.contains("value=true"));
    }

    @Override
    protected String getTag() {
        return TAG;
    }

    @Override
    protected String getComponentName() {
        return ToggleableTestTileService.getComponentName().flattenToString();
    }

    @Override
    protected TileService getTileServiceInstance() {
        return ToggleableTestTileService.getInstance();
    }

    /**
     * Waits for the TileService to be in the expected listening state. If it times out, it fails
     * the test
     * @param state desired listening state
     * @throws InterruptedException
     */
    @Override
    protected void waitForListening(boolean state) throws InterruptedException {
        int ct = 0;
        while (ToggleableTestTileService.isListening() != state && (ct++ < CHECK_RETRIES)) {
            Thread.sleep(CHECK_DELAY);
        }
        assertEquals(state, ToggleableTestTileService.isListening());
    }

    /**
     * Waits for the TileService to be in the expected connected state. If it times out, it fails
     * the test
     * @param state desired connected state
     * @throws InterruptedException
     */
    @Override
    protected void waitForConnected(boolean state) throws InterruptedException {
        int ct = 0;
        while (ToggleableTestTileService.isConnected() != state && (ct++ < CHECK_RETRIES)) {
            Thread.sleep(CHECK_DELAY);
        }
        assertEquals(state, ToggleableTestTileService.isConnected());
    }
}
