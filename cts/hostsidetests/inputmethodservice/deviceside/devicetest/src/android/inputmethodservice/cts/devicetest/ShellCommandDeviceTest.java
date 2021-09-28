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

package android.inputmethodservice.cts.devicetest;

import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.concurrent.TimeUnit;

/**
 * Test general shell commands implemented in InputMethodManagerService.
 */
@RunWith(AndroidJUnit4.class)
public class ShellCommandDeviceTest {
    private static final long TIMEOUT = TimeUnit.SECONDS.toMillis(5);

    private static void assertShellCommandThrowsException(String[] args) {
        final DirectShellCommand.Result result = DirectShellCommand.runSync(
                Context.INPUT_METHOD_SERVICE, args, TIMEOUT);
        assertNotNull(result);
        assertNotEquals(0, result.getCode());
        assertNotNull(result.getException());
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{}, null, receiver)}
     * returns {@link SecurityException}.
     */
    @Test
    public void testShellCommand() {
        assertShellCommandThrowsException(new String[]{});
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"ime"}, null, receiver)}
     * returns {@link SecurityException}.
     */
    @Test
    public void testShellCommandIme() {
        assertShellCommandThrowsException(new String[]{"ime"});
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"ime", "list"}, null,
     * receiver)} returns {@link SecurityException}.
     */
    @Test
    public void testShellCommandImeList() {
        assertShellCommandThrowsException(new String[]{"ime", "list"});
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"dump"}, null, receiver)}
     * returns {@link SecurityException}.
     */
    @Test
    public void testShellCommandDump() {
        assertShellCommandThrowsException(new String[]{"dump"});
    }

    /**
     * Make sure
     * {@code IInputMethodManager#shellCommand(in, out, err, new String[]{"help"}, null, receiver)}
     * returns {@link SecurityException}.
     */
    @Test
    public void testShellCommandHelp() {
        assertShellCommandThrowsException(new String[]{"help"});
    }
}
