/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */

package android.content.cts;

import static android.content.ContentResolver.QUERY_ARG_SQL_SELECTION;
import static android.content.ContentResolver.QUERY_ARG_SQL_SELECTION_ARGS;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.ContentProvider;
import android.content.ContentProviderOperation;
import android.content.ContentProviderResult;
import android.content.ContentValues;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;

import java.util.Objects;

@RunWith(AndroidJUnit4.class)
public class ContentProviderOperationTest {
    private static final Uri TEST_URI = Uri.parse("content://com.example");
    private static final Uri TEST_URI_RESULT = Uri.parse("content://com.example/12");
    private static final String TEST_SELECTION = "foo=?";
    private static final String[] TEST_SELECTION_ARGS = new String[] { "bar" };
    private static final String TEST_METHOD = "test_method";
    private static final String TEST_ARG = "test_arg";

    private static final ContentValues TEST_VALUES = new ContentValues();
    private static final Bundle TEST_EXTRAS = new Bundle();
    private static final Bundle TEST_EXTRAS_WITH_SQL = new Bundle();
    private static final Bundle TEST_EXTRAS_RESULT = new Bundle();

    static {
        TEST_VALUES.put("test_key", "test_value");

        TEST_EXTRAS.putString("test_key", "test_value");

        TEST_EXTRAS_WITH_SQL.putAll(TEST_EXTRAS);
        TEST_EXTRAS_WITH_SQL.putString(QUERY_ARG_SQL_SELECTION, TEST_SELECTION);
        TEST_EXTRAS_WITH_SQL.putStringArray(QUERY_ARG_SQL_SELECTION_ARGS, TEST_SELECTION_ARGS);

        TEST_EXTRAS_RESULT.putString("test_result", "42");
    }

    private static final ContentProviderResult[] TEST_RESULTS = new ContentProviderResult[] {
            new ContentProviderResult(TEST_URI_RESULT),
            new ContentProviderResult(84),
            new ContentProviderResult(TEST_EXTRAS_RESULT),
            new ContentProviderResult(new IllegalArgumentException()),
    };

    private ContentProvider provider;

    private ContentProviderOperation op;
    private ContentProviderResult res;

    @Before
    public void setUp() throws Exception {
        provider = mock(ContentProvider.class);
    }

    @Test
    public void testInsert() throws Exception {
        op = ContentProviderOperation.newInsert(TEST_URI)
                .withValues(TEST_VALUES)
                .withExtras(TEST_EXTRAS)
                .build();

        assertEquals(TEST_URI, op.getUri());
        assertTrue(op.isInsert());
        assertTrue(op.isWriteOperation());

        when(provider.insert(eq(TEST_URI), eq(TEST_VALUES), eqBundle(TEST_EXTRAS)))
                .thenReturn(TEST_URI_RESULT);
        res = op.apply(provider, null, 0);
        assertEquals(TEST_URI_RESULT, res.uri);
    }

    @Test
    public void testUpdate() throws Exception {
        op = ContentProviderOperation.newUpdate(TEST_URI)
                .withSelection(TEST_SELECTION, TEST_SELECTION_ARGS)
                .withValues(TEST_VALUES)
                .withExtras(TEST_EXTRAS)
                .build();

        assertEquals(TEST_URI, op.getUri());
        assertTrue(op.isUpdate());
        assertTrue(op.isWriteOperation());

        when(provider.update(eq(TEST_URI), eq(TEST_VALUES), eqBundle(TEST_EXTRAS_WITH_SQL)))
                .thenReturn(1);
        res = op.apply(provider, null, 0);
        assertEquals(1, (int) res.count);
    }

    @Test
    public void testDelete() throws Exception {
        op = ContentProviderOperation.newDelete(TEST_URI)
                .withSelection(TEST_SELECTION, TEST_SELECTION_ARGS)
                .withExtras(TEST_EXTRAS)
                .build();

        assertEquals(TEST_URI, op.getUri());
        assertTrue(op.isDelete());
        assertTrue(op.isWriteOperation());

        when(provider.delete(eq(TEST_URI), eqBundle(TEST_EXTRAS_WITH_SQL)))
                .thenReturn(1);
        res = op.apply(provider, null, 0);
        assertEquals(1, (int) res.count);
    }

    @Test
    public void testAssertQuery() throws Exception {
        op = ContentProviderOperation.newAssertQuery(TEST_URI)
                .withSelection(TEST_SELECTION, TEST_SELECTION_ARGS)
                .withExtras(TEST_EXTRAS)
                .withValues(TEST_VALUES)
                .build();

        assertEquals(TEST_URI, op.getUri());
        assertTrue(op.isAssertQuery());
        assertTrue(op.isReadOperation());

        final MatrixCursor cursor = new MatrixCursor(new String[] { "test_key" });
        cursor.addRow(new Object[] { "test_value" });

        when(provider.query(eq(TEST_URI), eq(new String[] { "test_key" }),
                eqBundle(TEST_EXTRAS_WITH_SQL), eq(null)))
                        .thenReturn(cursor);
        op.apply(provider, null, 0);
    }

    @Test
    public void testCall() throws Exception {
        op = ContentProviderOperation.newCall(TEST_URI, TEST_METHOD, TEST_ARG)
                .withExtras(TEST_EXTRAS)
                .build();

        assertEquals(TEST_URI, op.getUri());
        assertTrue(op.isCall());

        when(provider.call(eq(TEST_URI.getAuthority()), eq(TEST_METHOD),
                eq(TEST_ARG), eqBundle(TEST_EXTRAS)))
                        .thenReturn(TEST_EXTRAS_RESULT);
        res = op.apply(provider, null, 0);
        assertEquals(TEST_EXTRAS_RESULT, res.extras);
    }

    @Test
    public void testBackReferenceSelection() throws Exception {
        op = ContentProviderOperation.newDelete(TEST_URI)
                .withSelection(null, new String[] { "a", "b", "c", "d" })
                .withSelectionBackReference(0, 0)
                .withSelectionBackReference(1, 1)
                .withSelectionBackReference(2, 2, "test_result")
                .build();

        final String[] res = op.resolveSelectionArgsBackReferences(TEST_RESULTS,
                TEST_RESULTS.length);
        assertEquals("12", res[0]);
        assertEquals("84", res[1]);
        assertEquals("42", res[2]);
        assertEquals("d", res[3]);
    }

    @Test
    public void testBackReferenceValue() throws Exception {
        final ContentValues values = new ContentValues();
        values.put("a", "a");
        values.put("b", "b");
        values.put("c", "c");
        values.put("d", "d");

        op = ContentProviderOperation.newUpdate(TEST_URI)
                .withValues(values)
                .withValueBackReference("a", 0)
                .withValueBackReference("b", 1)
                .withValueBackReference("c", 2, "test_result")
                .build();

        final ContentValues res = op.resolveValueBackReferences(TEST_RESULTS,
                TEST_RESULTS.length);
        assertEquals(12L, (long) res.get("a"));
        assertEquals(84L, (long) res.get("b"));
        assertEquals("42", res.get("c"));
        assertEquals("d", res.get("d"));
    }

    @Test
    public void testBackReferenceExtra() throws Exception {
        final Bundle extras = new Bundle();
        extras.putString("a", "a");
        extras.putString("b", "b");
        extras.putString("c", "c");
        extras.putString("d", "d");

        op = ContentProviderOperation.newCall(TEST_URI, TEST_METHOD, TEST_ARG)
                .withExtras(extras)
                .withExtraBackReference("a", 0)
                .withExtraBackReference("b", 1)
                .withExtraBackReference("c", 2, "test_result")
                .build();

        final Bundle res = op.resolveExtrasBackReferences(TEST_RESULTS,
                TEST_RESULTS.length);
        assertEquals(12L, (long) res.get("a"));
        assertEquals(84L, (long) res.get("b"));
        assertEquals("42", res.get("c"));
        assertEquals("d", res.get("d"));
    }

    @Test
    public void testExceptionAllowed() throws Exception {
        op = ContentProviderOperation.newCall(TEST_URI, TEST_METHOD, TEST_ARG)
                .withExtras(TEST_EXTRAS)
                .withExceptionAllowed(true)
                .build();

        assertTrue(op.isExceptionAllowed());

        when(provider.call(eq(TEST_URI.getAuthority()), eq(TEST_METHOD),
                eq(TEST_ARG), eqBundle(TEST_EXTRAS)))
                        .thenThrow(new IllegalArgumentException());
        res = op.apply(provider, null, 0);
        assertTrue((res.exception instanceof IllegalArgumentException));
    }

    @Test
    public void testLayering() throws Exception {
        op = ContentProviderOperation.newAssertQuery(TEST_URI)
                .withSelection(TEST_SELECTION, TEST_SELECTION_ARGS)
                .withExtras(TEST_EXTRAS)
                .withExtra("test_key", "other_extra")
                .withValues(TEST_VALUES)
                .withValue("test_key", "other_value")
                .build();

        assertEquals("other_extra", op.resolveExtrasBackReferences(null, 0).getString("test_key"));
        assertEquals("other_value", op.resolveValueBackReferences(null, 0).getAsString("test_key"));
    }

    public static Bundle eqBundle(Bundle bundle) {
        return ArgumentMatchers.argThat((other) -> {
            // Ideally we'd use something like Bundle.kindofEquals() here, but
            // it doesn't perform deep equals inside String[] values, so the
            // best we can do is a simple string equality check
            return Objects.equals(bundle.toString(), other.toString());
        });
    }
}
