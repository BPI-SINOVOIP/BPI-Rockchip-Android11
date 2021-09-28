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

package com.android.cts.verifier.qstiles;

import android.view.View;
import android.view.ViewGroup;

import com.android.cts.verifier.R;

import java.util.ArrayList;
import java.util.List;

public class TileServiceVerifierActivity extends InteractiveVerifierActivity implements Runnable {

    @Override
    protected int getTitleResource() {
        return R.string.tiles_test;
    }

    @Override
    protected int getInstructionsResource() {
        return R.string.tiles_info;
    }

    @Override
    protected List<InteractiveTestCase> createTestItems() {
        ArrayList<InteractiveTestCase> list = new ArrayList<>();
        list.add(new SettingUpTile());
        list.add(new TileNotAvailable());
        list.add(new TileInCustomizer());
        list.add(new TearingDownTile());
        return list;
    }

    private class SettingUpTile extends InteractiveTestCase {

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.tiles_adding_tile);
        }

        @Override
        protected void test() {
            boolean result = setTileState(true);
            if (result) {
                status = PASS;
            } else {
                setFailed("Tile Service failed to enable");
            }
        }
    }

    // Tests
    private class TileNotAvailable extends InteractiveTestCase {
        @Override
        protected View inflate(ViewGroup parent) {
            return createUserPassFail(parent, R.string.tiles_not_added);

        }

        @Override
        protected void test() {
            status = WAIT_FOR_USER;
            next();
        }
    }

    // Tests
    private class TileInCustomizer extends InteractiveTestCase {
        @Override
        protected View inflate(ViewGroup parent) {
            return createUserPassFail(parent, R.string.tiles_in_customizer);

        }

        @Override
        protected void test() {
            status = WAIT_FOR_USER;
            next();
        }
    }

    private class TearingDownTile extends InteractiveTestCase {

        @Override
        protected View inflate(ViewGroup parent) {
            return createAutoItem(parent, R.string.tiles_removing_tile);
        }

        @Override
        protected void test() {
            boolean result = setTileState(false);
            if (result) {
                status = PASS;
            } else {
                setFailed("Tile Service failed to disable");
            }
        }
    }
}
