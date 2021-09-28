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

/**
 * @author Anatoly F. Bondarenko
 */

/**
 * Created on 24.02.2005
 */
package org.apache.harmony.jpda.tests.jdwp.ReferenceType;

import java.lang.reflect.Proxy;

import org.apache.harmony.jpda.tests.jdwp.Events.SourceDebugExtensionMockClass;
import org.apache.harmony.jpda.tests.share.JPDADebuggeeSynchronizer;
import org.apache.harmony.jpda.tests.share.SyncDebuggee;

public class SourceDebugExtensionDebuggee extends SyncDebuggee {

    @Override
    public void run() {
        // Create an instance of classWithSourceDebugExtension so the
        // SourceDebugExtension metadata can be reported back to the debugger.
        try {
            SourceDebugExtensionMockClass.class.getConstructor().newInstance();
        } catch (Exception e) {
            logWriter.println("--> Debuggee: Failed to instantiate " +
                              "SourceDebugExtensionMockClass: " + e);
        }

        // Instantiate a proxy whose name should contain "$Proxy".
        Class proxy = Proxy.getProxyClass(SomeInterface.class.getClassLoader(),
                                          new Class[] { SomeInterface.class });

        synchronizer.sendMessage(JPDADebuggeeSynchronizer.SGNL_READY);
        logWriter.println("--> Debuggee: SourceDebugExtensionDebuggee...");
        synchronizer.receiveMessage(JPDADebuggeeSynchronizer.SGNL_CONTINUE);
    }

    interface SomeInterface {}

    public static void main(String [] args) {
        runDebuggee(SourceDebugExtensionDebuggee.class);
    }

}
