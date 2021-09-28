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

package com.android.tv.dvr.provider;

import static com.google.common.truth.Truth.assertThat;

import android.os.Build.VERSION_CODES;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for {@link DvrDatabaseHelper} */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = VERSION_CODES.N)
public class DvrDatabaseHelperTest {

    @Test
    public void testSqlCreateSchedules() {
        assertThat(DvrDatabaseHelper.SQL_CREATE_SCHEDULES).isEqualTo(
                "CREATE TABLE schedules("
                        + "_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        + "priority INTEGER DEFAULT 4611686018427387903,"
                        + "type TEXT NOT NULL,"
                        + "input_id TEXT NOT NULL,"
                        + "channel_id INTEGER NOT NULL,"
                        + "program_id INTEGER,"
                        + "program_title TEXT,"
                        + "start_time_utc_millis INTEGER NOT NULL,"
                        + "end_time_utc_millis INTEGER NOT NULL,"
                        + "season_number TEXT,"
                        + "episode_number TEXT,"
                        + "episode_title TEXT,"
                        + "program_description TEXT,"
                        + "program_long_description TEXT,"
                        + "program_poster_art_uri TEXT,"
                        + "program_thumbnail_uri TEXT,"
                        + "state TEXT NOT NULL,"
                        + "failed_reason TEXT,"
                        + "series_recording_id INTEGER,"
                        + "FOREIGN KEY(series_recording_id) "
                        + "REFERENCES series_recording(_id) "
                        + "ON UPDATE CASCADE ON DELETE SET NULL);"
        );
    }

    @Test
    public void testSqlDropSchedules() {
        assertThat(DvrDatabaseHelper.SQL_DROP_SCHEDULES).isEqualTo(
                "DROP TABLE IF EXISTS schedules"
        );
    }

    @Test
    public void testSqlCreateSeriesRecordings() {
        assertThat(DvrDatabaseHelper.SQL_CREATE_SERIES_RECORDINGS).isEqualTo(
                "CREATE TABLE series_recording("
                        + "_id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        + "priority INTEGER DEFAULT 4611686018427387903,"
                        + "title TEXT NOT NULL,"
                        + "short_description TEXT,"
                        + "long_description TEXT,"
                        + "input_id TEXT NOT NULL,"
                        + "channel_id INTEGER NOT NULL,"
                        + "series_id TEXT NOT NULL,"
                        + "start_from_season INTEGER DEFAULT -1,"
                        + "start_from_episode INTEGER DEFAULT -1,"
                        + "channel_option TEXT DEFAULT OPTION_CHANNEL_ONE,"
                        + "canonical_genre TEXT,"
                        + "poster_uri TEXT,"
                        + "photo_uri TEXT,"
                        + "state TEXT);"
        );
    }

    @Test
    public void testSqlDropSeriesRecordings() {
        assertThat(DvrDatabaseHelper.SQL_DROP_SERIES_RECORDINGS).isEqualTo(
                "DROP TABLE IF EXISTS series_recording"
        );
    }
}
