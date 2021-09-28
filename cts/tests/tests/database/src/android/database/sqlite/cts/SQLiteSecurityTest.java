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

package android.database.sqlite.cts;


import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteDatabaseCorruptException;
import android.test.AndroidTestCase;

/**
 * This CTS test verifies Magellan SQLite Security Vulnerability.
 * Without the fix, the last statement in each test case triggers a segmentation fault and the test
 * fails.
 * With the fix, the last statement in each test case triggers SQLiteDatabaseCorruptException with
 * message "database disk image is malformed (code 267 SQLITE_CORRUPT_VTAB)", this is expected
 * behavior that we are crashing and we are not leaking data.
 */
public class SQLiteSecurityTest extends AndroidTestCase {
    private static final String DATABASE_NAME = "database_test.db";

    private SQLiteDatabase mDatabase;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        getContext().deleteDatabase(DATABASE_NAME);
        mDatabase = getContext().openOrCreateDatabase(DATABASE_NAME, Context.MODE_PRIVATE,
              null);
        assertNotNull(mDatabase);
    }

    @Override
    protected void tearDown() throws Exception {
        mDatabase.close();
        getContext().deleteDatabase(DATABASE_NAME);

        super.tearDown();
    }

    public void testScript1() {
        mDatabase.beginTransaction();
        mDatabase.execSQL("CREATE VIRTUAL TABLE ft USING fts3;");
        mDatabase.execSQL("INSERT INTO ft_content VALUES(1,'aback');");
        mDatabase.execSQL("INSERT INTO ft_content VALUES(2,'abaft');");
        mDatabase.execSQL("INSERT INTO ft_content VALUES(3,'abandon');");
        mDatabase.execSQL("INSERT INTO ft_segdir VALUES(0,0,0,0,'0 29',X"
            + "'0005616261636b03010200ffffffff070266740302020003046e646f6e03030200');");
        mDatabase.setTransactionSuccessful();
        mDatabase.endTransaction();
        try {
            mDatabase.execSQL("SELECT * FROM ft WHERE ft MATCH 'abandon';");
        } catch (SQLiteDatabaseCorruptException e) {
            return;
        }
        fail("Expecting a SQLiteDatabaseCorruptException");
    }

    public void testScript2() {
      mDatabase.beginTransaction();
      mDatabase.execSQL("CREATE VIRTUAL TABLE ft USING fts3;");
      mDatabase.execSQL("INSERT INTO ft_segments VALUES(1,"
          + "X'0004616263300301020003013103020200040130030b0200040131030c0200');");
      mDatabase.execSQL("INSERT INTO ft_segments VALUES(2,"
          + "X'00056162633132030d0200040133030e0200040134030f020004013503100200');");
      mDatabase.execSQL("INSERT INTO ft_segments VALUES(3,"
          + "X'0005616263313603110200040137031202000401380313020004013903140200');");
      mDatabase.execSQL("INSERT INTO ft_segments VALUES(4,"
          + "X'00046162633203030200030133030402000301340305020003013503060200');");
      mDatabase.execSQL("INSERT INTO ft_segments VALUES(5,"
          + "X'000461626336030702000301370308020003013803090200030139030a0200');");
      mDatabase.execSQL("INSERT INTO ft_segdir "
          + "VALUES(0,0,1,5,'5 157',X'0101056162633132ffffffff070236030132030136');");
      mDatabase.setTransactionSuccessful();
      mDatabase.endTransaction();
      try {
          mDatabase.execSQL("SELECT * FROM ft WHERE ft MATCH 'abc20';");
      } catch (SQLiteDatabaseCorruptException e) {
          return;
      }
      fail("Expecting a SQLiteDatabaseCorruptException");
    }

    public void testScript3() {
      mDatabase.beginTransaction();
      mDatabase.execSQL("CREATE VIRTUAL TABLE ft USING fts4;");
      mDatabase.execSQL("INSERT INTO ft_segments VALUES"
          + "(1,X'00046162633003010200040178030202000501780303020003013103040200');");
      mDatabase.execSQL("INSERT INTO ft_segments VALUES"
          + "(2,X'00056162633130031f0200ffffffff07ff5566740302020003046e646f6e03030200');");
      mDatabase.execSQL("INSERT INTO ft_segments VALUES(384,NULL);");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES"
          + "(0,0,0,0,'0 24',X'000561626331780305020005017803060200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES"
          + " (0,1,0,0,'0 24',X'000461626332030702000401780308020005017803090200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES"
          + "(0,2,0,0,'0 24',X'000461626333030a0200040178030b0200050178030c0200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES" +
          "(0,3,0,0,'0 24',X'000461626334030d0200040178030e0200050178030f0200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES"
          + "(0,4,0,0,'0 24',X'000461626335031002000401780311020005017803120200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES"
          + "(0,5,0,0,'0 24',X'000461626336031302000401780314020005017803150200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES"
          + "(0,6,0,0,'0 24',X'000461626337031602000401780317020005017803180200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES"
          + "(0,7,0,0,'0 24',X'00046162633803190200040178031a0200050178031b0200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES"
          + "(0,8,0,0,'0 24',X'000461626339031c0200040178031d0200050178031e0200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES"
          + "(0,9,0,0,'0 25',X'00066162633130780320020006017803210200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES"
          + "(0,10,0,0,'0 25',X'00056162633131032202000501780323020006017803240200');");
      mDatabase.execSQL("INSERT INTO ft_segdir VALUES(1,0,1,2,'384 -42',X'0101056162633130');");
      mDatabase.execSQL("INSERT INTO ft_stat VALUES(1,X'000b');");
      mDatabase.execSQL("PRAGMA writable_schema=OFF;");
      mDatabase.setTransactionSuccessful();
      mDatabase.endTransaction();
      try {
          mDatabase.execSQL("INSERT INTO ft(ft) VALUES('merge=1,4');");
      } catch (SQLiteDatabaseCorruptException e) {
          return;
      }
      fail("Expecting a SQLiteDatabaseCorruptException");
    }
}











