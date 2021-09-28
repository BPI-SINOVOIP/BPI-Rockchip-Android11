/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

package org.apache.harmony.jpda.tests.jdwp.Events;

import org.apache.harmony.jpda.tests.framework.TestErrorException;
import org.apache.harmony.jpda.tests.framework.jdwp.CommandPacket;
import org.apache.harmony.jpda.tests.framework.jdwp.JDWPCommands;
import org.apache.harmony.jpda.tests.framework.jdwp.JDWPConstants;
import org.apache.harmony.jpda.tests.framework.jdwp.ParsedEvent;
import org.apache.harmony.jpda.tests.framework.jdwp.ParsedEvent.Event_MONITOR_CONTENDED_ENTERED;
import org.apache.harmony.jpda.tests.framework.jdwp.ReplyPacket;
import org.apache.harmony.jpda.tests.framework.jdwp.Value;
import org.apache.harmony.jpda.tests.jdwp.share.JDWPSyncTestCase;
import org.apache.harmony.jpda.tests.share.JPDADebuggeeSynchronizer;

public class MonitorContendedEnteredTest extends JDWPSyncTestCase {
    String monitorSignature = getClassSignature(MonitorWaitMockMonitor.class);

    @Override
    protected String getDebuggeeClassName() {
        return MonitorContendedEnterAndEnteredDebuggee.class.getName();
    }

    public void testMonitorContendedEnteredForClassMatch() {
        synchronizer.receiveMessage(JPDADebuggeeSynchronizer.SGNL_READY);

        // Set MONITOR_CONTENDED_ENTERED request for MonitorContendedEnterAndEnteredDebuggee
        logWriter.println("==> Debuggee class pattern to match = " + getDebuggeeClassName() + "*");
        debuggeeWrapper.vmMirror.setMonitorContendedEnteredForClassMatch(getDebuggeeClassName()+"*");

        // Verify received event
        verifyEvent();
    }

    private void verifyEvent(){
        // Inform debuggee that the request has been set
        logWriter.println("==> Request has been set.");
        synchronizer.sendMessage(JPDADebuggeeSynchronizer.SGNL_CONTINUE);
        // Debuggee thinks the monitor is contended.
        synchronizer.receiveMessage(JPDADebuggeeSynchronizer.SGNL_READY);

        // Wait for the other thread to look asleep.
        boolean hasWaiters;
        long classID = getClassIDBySignature(getDebuggeeClassSignature());
        long cnt = 0;
        String fieldName = "lock";
        long fld_id = checkField(classID, fieldName);
        do {
            CommandPacket getValuesCommand = new CommandPacket(
                    JDWPCommands.ReferenceTypeCommandSet.CommandSetID,
                    JDWPCommands.ReferenceTypeCommandSet.GetValuesCommand);
            getValuesCommand.setNextValueAsReferenceTypeID(classID);
            getValuesCommand.setNextValueAsInt(1);
            getValuesCommand.setNextValueAsFieldID(fld_id);
            ReplyPacket getValuesReply = debuggeeWrapper.vmMirror.performCommand(getValuesCommand);
            checkReplyPacket(getValuesReply, "ReferenceType::GetValues command");
            getValuesReply.getNextValueAsInt();  // num-replies
            Value lkvalue = getValuesReply.getNextValueAsValue();
            long lk = lkvalue.getLongValue();
            CommandPacket monitorInfoCmd = new CommandPacket(
                    JDWPCommands.ObjectReferenceCommandSet.CommandSetID,
                    JDWPCommands.ObjectReferenceCommandSet.MonitorInfoCommand);
            monitorInfoCmd.setNextValueAsObjectID(lk);
            ReplyPacket monInfoReply = debuggeeWrapper.vmMirror.performCommand(monitorInfoCmd);
            checkReplyPacket(monInfoReply, "ObjectReference::MonitorInfo command");
            monInfoReply.getNextValueAsThreadID();  // owner
            monInfoReply.getNextValueAsInt();  // entryCount
            hasWaiters = monInfoReply.getNextValueAsInt() != 0;
        } while (!hasWaiters);
        logWriter.println("==> Monitor has waiter.");
        // Wake up the blocking thread. Its job is done.
        synchronizer.sendMessage(JPDADebuggeeSynchronizer.SGNL_CONTINUE);

        // Receive event of MONITOR_CONTENDED_ENTERED
        logWriter.println("==> Receive Event.");
        CommandPacket receiveEvent = null;
        try {
            receiveEvent = debuggeeWrapper.vmMirror.receiveEvent();
        } catch (TestErrorException e) {
            printErrorAndFail("There is no event received.");
        }
        ParsedEvent[] parsedEvents = ParsedEvent.parseEventPacket(receiveEvent);
        Event_MONITOR_CONTENDED_ENTERED event = (ParsedEvent.Event_MONITOR_CONTENDED_ENTERED)parsedEvents[0];

        assertEquals("Invalid number of events,", 1, parsedEvents.length);
        assertEquals("Invalid event kind,", JDWPConstants.EventKind.MONITOR_CONTENDED_ENTERED, event.getEventKind()
                , JDWPConstants.EventKind.getName(JDWPConstants.EventKind.MONITOR_CONTENDED_ENTERED)
                , JDWPConstants.EventKind.getName(event.getEventKind()));
        logWriter.println("==> CHECK: Event Kind: " + JDWPConstants.EventKind.getName(JDWPConstants.EventKind.MONITOR_CONTENDED_ENTERED));

        // Getting ID of the tested thread
        logWriter.println("==> Get testedThreadID...");
        long testedThreadID = debuggeeWrapper.vmMirror
                .getThreadID(MonitorContendedEnterAndEnteredDebuggee.TESTED_THREAD);
        assertEquals("Invalid tested thread id: ", testedThreadID
              , event.getThreadID());
        logWriter.println("==> CHECK: tested blocked thread id: " + testedThreadID );
        
        // Get monitor object from event packet
        long objID = event.getTaggedObject().objectID;
        // Check the ReferenceType of monitor object
        long refID = debuggeeWrapper.vmMirror.getReferenceType(objID);
        String actualSignature =  debuggeeWrapper.vmMirror.getReferenceTypeSignature(refID);
        assertEquals("Invalid monitor class signature: ", monitorSignature
                , actualSignature);
        logWriter.println("==> CHECK: monitor class signature: " + actualSignature);
    }

}
