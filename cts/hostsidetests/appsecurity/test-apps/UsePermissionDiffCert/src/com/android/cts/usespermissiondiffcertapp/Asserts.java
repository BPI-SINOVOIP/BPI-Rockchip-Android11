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

package com.android.cts.usespermissiondiffcertapp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.ClipData;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.UriPermission;
import android.database.Cursor;
import android.net.Uri;

import androidx.test.InstrumentationRegistry;

import java.io.IOException;
import java.util.List;

public class Asserts {
    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    static void assertAccess(ClipData clip, int mode) {
        for (int i = 0; i < clip.getItemCount(); i++) {
            ClipData.Item item = clip.getItemAt(i);
            Uri uri = item.getUri();
            if (uri != null) {
                assertAccess(uri, mode);
            } else {
                Intent intent = item.getIntent();
                uri = intent.getData();
                if (uri != null) {
                    assertAccess(uri, mode);
                }
                ClipData intentClip = intent.getClipData();
                if (intentClip != null) {
                    assertAccess(intentClip, mode);
                }
            }
        }
    }

    static void assertAccess(Uri uri, int mode) {
        if ((mode & Intent.FLAG_GRANT_READ_URI_PERMISSION) != 0) {
            assertReadingContentUriAllowed(uri);
        } else {
            assertReadingContentUriNotAllowed(uri, null);
        }
        if ((mode & Intent.FLAG_GRANT_WRITE_URI_PERMISSION) != 0) {
            assertWritingContentUriAllowed(uri);
        } else {
            assertWritingContentUriNotAllowed(uri, null);
        }
    }

    static void assertReadingContentUriNotAllowed(Uri uri, String msg) {
        try {
            getContext().getContentResolver().query(uri, null, null, null, null);
            fail("expected SecurityException reading " + uri + ": " + msg);
        } catch (SecurityException expected) {
            assertNotNull("security exception's error message.", expected.getMessage());
        }
    }

    static void assertReadingContentUriAllowed(Uri uri) {
        try {
            getContext().getContentResolver().query(uri, null, null, null, null);
        } catch (SecurityException e) {
            fail("unexpected SecurityException reading " + uri + ": " + e.getMessage());
        }
    }

    static void assertReadingClipNotAllowed(ClipData clip) {
        assertReadingClipNotAllowed(clip, null);
    }

    static void assertReadingClipNotAllowed(ClipData clip, String msg) {
        for (int i=0; i<clip.getItemCount(); i++) {
            ClipData.Item item = clip.getItemAt(i);
            Uri uri = item.getUri();
            if (uri != null) {
                assertReadingContentUriNotAllowed(uri, msg);
            } else {
                Intent intent = item.getIntent();
                uri = intent.getData();
                if (uri != null) {
                    assertReadingContentUriNotAllowed(uri, msg);
                }
                ClipData intentClip = intent.getClipData();
                if (intentClip != null) {
                    assertReadingClipNotAllowed(intentClip, msg);
                }
            }
        }
    }

    static void assertOpenFileDescriptorModeNotAllowed(Uri uri, String msg, String mode) {
        try {
            getContext().getContentResolver().openFileDescriptor(uri, mode).close();
            fail("expected SecurityException writing " + uri + ": " + msg);
        } catch (IOException e) {
            throw new IllegalStateException(e);
        } catch (SecurityException expected) {
            assertNotNull("security exception's error message.", expected.getMessage());
        }
    }

    static void assertContentUriAllowed(Uri uri) {
        assertReadingContentUriAllowed(uri);
        assertWritingContentUriAllowed(uri);
    }

    static void assertContentUriNotAllowed(Uri uri, String msg) {
        assertReadingContentUriNotAllowed(uri, msg);
        assertWritingContentUriNotAllowed(uri, msg);
    }

    static void assertWritingContentUriNotAllowed(Uri uri, String msg) {
        final ContentResolver resolver = getContext().getContentResolver();
        try {
            resolver.insert(uri, new ContentValues());
            fail("expected SecurityException inserting " + uri + ": " + msg);
        } catch (SecurityException expected) {
            assertNotNull("security exception's error message.", expected.getMessage());
        }

        try {
            resolver.update(uri, new ContentValues(), null, null);
            fail("expected SecurityException updating " + uri + ": " + msg);
        } catch (SecurityException expected) {
            assertNotNull("security exception's error message.", expected.getMessage());
        }

        try {
            resolver.delete(uri, null, null);
            fail("expected SecurityException deleting " + uri + ": " + msg);
        } catch (SecurityException expected) {
            assertNotNull("security exception's error message.", expected.getMessage());
        }

        try {
            getContext().getContentResolver().openOutputStream(uri).close();
            fail("expected SecurityException writing " + uri + ": " + msg);
        } catch (IOException e) {
            throw new IllegalStateException(e);
        } catch (SecurityException expected) {
            assertNotNull("security exception's error message.", expected.getMessage());
        }

        assertOpenFileDescriptorModeNotAllowed(uri, msg, "w");
        assertOpenFileDescriptorModeNotAllowed(uri, msg, "wt");
        assertOpenFileDescriptorModeNotAllowed(uri, msg, "wa");
        assertOpenFileDescriptorModeNotAllowed(uri, msg, "rw");
        assertOpenFileDescriptorModeNotAllowed(uri, msg, "rwt");
    }

    static void assertWritingContentUriAllowed(Uri uri) {
        final ContentResolver resolver = getContext().getContentResolver();
        try {
            resolver.insert(uri, new ContentValues());
            resolver.update(uri, new ContentValues(), null, null);
            resolver.delete(uri, null, null);

            resolver.openOutputStream(uri).close();
            resolver.openFileDescriptor(uri, "w").close();
            resolver.openFileDescriptor(uri, "wt").close();
            resolver.openFileDescriptor(uri, "wa").close();
            resolver.openFileDescriptor(uri, "rw").close();
            resolver.openFileDescriptor(uri, "rwt").close();
        } catch (IOException e) {
            fail("unexpected IOException writing " + uri + ": " + e.getMessage());
        } catch (SecurityException e) {
            fail("unexpected SecurityException writing " + uri + ": " + e.getMessage());
        }
    }

    static void assertWritingClipNotAllowed(ClipData clip) {
        assertWritingClipNotAllowed(clip, null);
    }

    static void assertWritingClipNotAllowed(ClipData clip, String msg) {
        for (int i=0; i<clip.getItemCount(); i++) {
            ClipData.Item item = clip.getItemAt(i);
            Uri uri = item.getUri();
            if (uri != null) {
                assertWritingContentUriNotAllowed(uri, msg);
            } else {
                Intent intent = item.getIntent();
                uri = intent.getData();
                if (uri != null) {
                    assertWritingContentUriNotAllowed(uri, msg);
                }
                ClipData intentClip = intent.getClipData();
                if (intentClip != null) {
                    assertWritingClipNotAllowed(intentClip, msg);
                }
            }
        }
    }

    static void assertReadingClipAllowed(ClipData clip) {
        for (int i=0; i<clip.getItemCount(); i++) {
            ClipData.Item item = clip.getItemAt(i);
            Uri uri = item.getUri();
            if (uri != null) {
                Cursor c = getContext().getContentResolver().query(uri,
                        null, null, null, null);
                if (c != null) {
                    c.close();
                }
            } else {
                Intent intent = item.getIntent();
                uri = intent.getData();
                if (uri != null) {
                    Cursor c = getContext().getContentResolver().query(uri,
                            null, null, null, null);
                    if (c != null) {
                        c.close();
                    }
                }
                ClipData intentClip = intent.getClipData();
                if (intentClip != null) {
                    assertReadingClipAllowed(intentClip);
                }
            }
        }
    }

    static void assertWritingClipAllowed(ClipData clip) {
        for (int i=0; i<clip.getItemCount(); i++) {
            ClipData.Item item = clip.getItemAt(i);
            Uri uri = item.getUri();
            if (uri != null) {
                getContext().getContentResolver().insert(uri, new ContentValues());
            } else {
                Intent intent = item.getIntent();
                uri = intent.getData();
                if (uri != null) {
                    getContext().getContentResolver().insert(uri, new ContentValues());
                }
                ClipData intentClip = intent.getClipData();
                if (intentClip != null) {
                    assertWritingClipAllowed(intentClip);
                }
            }
        }
    }


    static void assertClipDataEquals(ClipData expected, ClipData actual) {
        assertEquals(expected.getItemCount(), actual.getItemCount());
        for (int i = 0; i < expected.getItemCount(); i++) {
            final ClipData.Item expectedItem = expected.getItemAt(i);
            final ClipData.Item actualItem = actual.getItemAt(i);
            assertEquals(expectedItem.getText(), actualItem.getText());
            assertEquals(expectedItem.getHtmlText(), actualItem.getHtmlText());
            assertEquals(expectedItem.getIntent(), actualItem.getIntent());
            assertEquals(expectedItem.getUri(), actualItem.getUri());
        }
    }

    static void assertNoPersistedUriPermission() {
        assertPersistedUriPermission(null, 0, -1, -1);
    }

    static void assertPersistedUriPermission(Uri uri, int flags, long before, long after) {
        // Assert local
        final List<UriPermission> perms = getContext()
                .getContentResolver().getPersistedUriPermissions();
        if (uri != null) {
            assertEquals("expected exactly one permission", 1, perms.size());

            final UriPermission perm = perms.get(0);
            assertEquals("unexpected uri", uri, perm.getUri());

            final long actual = perm.getPersistedTime();
            if (before != -1) {
                assertTrue("found " + actual + " before " + before, actual >= before);
            }
            if (after != -1) {
                assertTrue("found " + actual + " after " + after, actual <= after);
            }

            final boolean expectedRead = (flags & Intent.FLAG_GRANT_READ_URI_PERMISSION) != 0;
            final boolean expectedWrite = (flags & Intent.FLAG_GRANT_WRITE_URI_PERMISSION) != 0;
            assertEquals("unexpected read status", expectedRead, perm.isReadPermission());
            assertEquals("unexpected write status", expectedWrite, perm.isWritePermission());

        } else {
            assertEquals("expected zero permissions", 0, perms.size());
        }

        Utils.verifyOutgoingPersisted(uri);
    }
}
