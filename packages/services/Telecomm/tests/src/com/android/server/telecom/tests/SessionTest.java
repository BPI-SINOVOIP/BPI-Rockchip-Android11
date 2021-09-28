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

package com.android.server.telecom.tests;

import static junit.framework.Assert.fail;

import android.telecom.Log;
import android.telecom.Logging.Session;
import android.test.suitebuilder.annotation.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

/**
 * Unit tests for android.telecom.Logging.Session
 */

@RunWith(JUnit4.class)
public class SessionTest extends TelecomTestCase {

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();
    }

    @Override
    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    /**
     * Ensure creating two sessions that are parent/child of each other does not lead to a crash
     * or infinite recursion when using Session#printFullSessionTree.
     */
    @SmallTest
    @Test
    public void testRecursion_printFullSessionTree() {
        Log.startSession("testParent");
        // Running in the same thread, so mark as invisible subsession
        Session childSession = Log.getSessionManager()
                .createSubsession(true /*isStartedFromActiveSession*/);
        Log.continueSession(childSession, "child");
        Session parentSession = childSession.getParentSession();
        // Create a circular dependency and ensure we do not crash
        parentSession.setParentSession(childSession);
        childSession.addChild(parentSession);

        // Make sure calling these methods does not result in a crash
        try {
            parentSession.printFullSessionTree();
            childSession.printFullSessionTree();
        } catch (Exception e) {
            fail("Exception: " + e.getMessage());
        } finally {
            // End child
            Log.endSession();
            // End parent
            Log.endSession();
        }
    }

    /**
     * Ensure creating two sessions that are parent/child of each other does not lead to a crash
     * or infinite recursion when using Session#getFullMethodPath.
     */
    @SmallTest
    @Test
    public void testRecursion_getFullMethodPath() {
        Log.startSession("testParent");
        // Running in the same thread, so mark as invisible subsession
        Session childSession = Log.getSessionManager()
                .createSubsession(true /*isStartedFromActiveSession*/);
        Log.continueSession(childSession, "child");
        Session parentSession = childSession.getParentSession();
        // Create a circular dependency and ensure we do not crash
        parentSession.setParentSession(childSession);
        childSession.addChild(parentSession);

        // Make sure calling these methods does not result in a crash
        try {
            parentSession.getFullMethodPath(false /*truncatePath*/);
            childSession.getFullMethodPath(false /*truncatePath*/);
        } catch (Exception e) {
            fail("Exception: " + e.getMessage());
        } finally {
            // End child
            Log.endSession();
            // End parent
            Log.endSession();
        }
    }

    /**
     * Ensure creating two sessions that are parent/child of each other does not lead to a crash
     * or infinite recursion when using Session#getFullMethodPath.
     */
    @SmallTest
    @Test
    public void testRecursion_getFullMethodPathTruncated() {
        Log.startSession("testParent");
        // Running in the same thread, so mark as invisible subsession
        Session childSession = Log.getSessionManager()
                .createSubsession(true /*isStartedFromActiveSession*/);
        Log.continueSession(childSession, "child");
        Session parentSession = childSession.getParentSession();
        // Create a circular dependency and ensure we do not crash
        parentSession.setParentSession(childSession);
        childSession.addChild(parentSession);

        // Make sure calling these methods does not result in a crash
        try {
            parentSession.getFullMethodPath(true /*truncatePath*/);
            childSession.getFullMethodPath(true /*truncatePath*/);
        } catch (Exception e) {
            fail("Exception: " + e.getMessage());
        } finally {
            // End child
            Log.endSession();
            // End parent
            Log.endSession();
        }
    }

    /**
     * Ensure creating two sessions that are parent/child of each other does not lead to a crash
     * or infinite recursion when using Session#toString.
     */
    @SmallTest
    @Test
    public void testRecursion_toString() {
        Log.startSession("testParent");
        // Running in the same thread, so mark as invisible subsession
        Session childSession = Log.getSessionManager()
                .createSubsession(true /*isStartedFromActiveSession*/);
        Log.continueSession(childSession, "child");
        Session parentSession = childSession.getParentSession();
        // Create a circular dependency and ensure we do not crash
        parentSession.setParentSession(childSession);
        childSession.addChild(parentSession);

        // Make sure calling these methods does not result in a crash
        try {

            parentSession.toString();
            childSession.toString();
        } catch (Exception e) {
            fail("Exception: " + e.getMessage());
        } finally {
            // End child
            Log.endSession();
            // End parent
            Log.endSession();
        }
    }

    /**
     * Ensure creating two sessions that are parent/child of each other does not lead to a crash
     * or infinite recursion when using Session#getInfo.
     */
    @SmallTest
    @Test
    public void testRecursion_getInfo() {
        Log.startSession("testParent");
        // Running in the same thread, so mark as invisible subsession
        Session childSession = Log.getSessionManager()
                .createSubsession(true /*isStartedFromActiveSession*/);
        Log.continueSession(childSession, "child");
        Session parentSession = childSession.getParentSession();
        // Create a circular dependency and ensure we do not crash
        parentSession.setParentSession(childSession);
        childSession.addChild(parentSession);

        // Make sure calling these methods does not result in a crash
        try {
            Session.Info.getInfo(parentSession);
            Session.Info.getInfo(childSession);
        } catch (Exception e) {
            fail("Exception: " + e.getMessage());
        } finally {
            // End child
            Log.endSession();
            // End parent
            Log.endSession();
        }
    }

    /**
     * Ensure creating two sessions that are parent/child of each other does not lead to a crash
     * or infinite recursion in the general case.
     */
    @SmallTest
    @Test
    public void testRecursion() {
        Session parentSession =  createTestSession("parent", "p");
        Session childSession =  createTestSession("child", "c");
        // Create a circular dependency
        parentSession.addChild(childSession);
        childSession.addChild(parentSession);
        parentSession.setParentSession(childSession);
        childSession.setParentSession(parentSession);

        // Make sure calling these methods does not result in a crash
        try {
            parentSession.printFullSessionTree();
            childSession.printFullSessionTree();
            parentSession.getFullMethodPath(false /*truncatePath*/);
            childSession.getFullMethodPath(false /*truncatePath*/);
            parentSession.getFullMethodPath(true /*truncatePath*/);
            childSession.getFullMethodPath(true /*truncatePath*/);
            parentSession.toString();
            childSession.toString();
            Session.Info.getInfo(parentSession);
            Session.Info.getInfo(childSession);
        } catch (Exception e) {
            fail("Exception: " + e.getMessage());
        }
    }

    private Session createTestSession(String name, String methodName) {
        return new Session(name, methodName, 0, false, null);
    }
}
