/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.tradefed.sandbox;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.command.CommandRunner.ExitCode;
import com.android.tradefed.command.ICommandScheduler;
import com.android.tradefed.command.ICommandScheduler.IScheduledInvocationListener;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.device.FreeDeviceState;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.NoDeviceException;
import com.android.tradefed.invoker.IInvocationContext;
import com.android.tradefed.invoker.InvocationContext;
import com.android.tradefed.invoker.proto.InvocationContext.Context;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.SerializationUtil;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/** Runner associated with a {@link TradefedSandbox} that will allow executing the sandbox. */
public class TradefedSandboxRunner {
    public static final String EXCEPTION_KEY = "serialized_exception";
    public static final String DEBUG_THREAD_KEY = "debug_thread";

    private ICommandScheduler mScheduler;
    private ExitCode mErrorCode = ExitCode.NO_ERROR;

    public TradefedSandboxRunner() {}

    public ExitCode getErrorCode() {
        return mErrorCode;
    }

    /** Initialize the required global configuration. */
    @VisibleForTesting
    void initGlobalConfig(String[] args) throws ConfigurationException {
        GlobalConfiguration.createGlobalConfiguration(args);
    }

    /** Get the {@link ICommandScheduler} instance from the global configuration. */
    @VisibleForTesting
    ICommandScheduler getCommandScheduler() {
        return GlobalConfiguration.getInstance().getCommandScheduler();
    }

    /** Prints the exception stack to stderr. */
    @VisibleForTesting
    void printStackTrace(Throwable e) {
        e.printStackTrace();
        File serializedException = null;
        try {
            serializedException = SerializationUtil.serialize(e);
            JSONObject json = new JSONObject();
            json.put(EXCEPTION_KEY, serializedException.getAbsolutePath());
            System.err.println(json.toString());
        } catch (IOException | JSONException io) {
            io.printStackTrace();
            FileUtil.deleteFile(serializedException);
        }
    }

    /**
     * The main method to run the command.
     *
     * @param args the config name to run and its options
     */
    public void run(String[] args) {
        if (System.getenv(DEBUG_THREAD_KEY) != null) {
            new DebugThread().start();
        }
        List<String> argList = new ArrayList<>(Arrays.asList(args));
        IInvocationContext context = null;

        if (argList.size() < 2) {
            mErrorCode = ExitCode.THROWABLE_EXCEPTION;
            printStackTrace(
                    new RuntimeException("TradefedContainerRunner expect at least 2 args."));
            return;
        }

        File contextFile = new File(argList.remove(0));
        try {
            Context c = Context.parseDelimitedFrom(new FileInputStream(contextFile));
            context = InvocationContext.fromProto(c);
        } catch (IOException e) {
            // Fallback to compatible old way
            // TODO: Delete when parent has been deployed.
            try {
                context = (IInvocationContext) SerializationUtil.deserialize(contextFile, false);
            } catch (IOException e2) {
                printStackTrace(e);
                printStackTrace(e2);
                mErrorCode = ExitCode.THROWABLE_EXCEPTION;
                return;
            }
        }

        try {
            initGlobalConfig(new String[] {});
            mScheduler = getCommandScheduler();
            mScheduler.start();
            mScheduler.execCommand(
                    context, new StubScheduledInvocationListener(), argList.toArray(new String[0]));
        } catch (NoDeviceException e) {
            printStackTrace(e);
            mErrorCode = ExitCode.NO_DEVICE_ALLOCATED;
        } catch (ConfigurationException e) {
            printStackTrace(e);
            mErrorCode = ExitCode.CONFIG_EXCEPTION;
        } finally {
            mScheduler.shutdownOnEmpty();
        }
        try {
            mScheduler.join();
            // If no error code has been raised yet, we checked the invocation error code.
            if (ExitCode.NO_ERROR.equals(mErrorCode)) {
                mErrorCode = mScheduler.getLastInvocationExitCode();
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
            mErrorCode = ExitCode.THROWABLE_EXCEPTION;
        }
        if (!ExitCode.NO_ERROR.equals(mErrorCode)
                && mScheduler.getLastInvocationThrowable() != null) {
            // Print error to the stderr so that it can be recovered.
            printStackTrace(mScheduler.getLastInvocationThrowable());
        }
    }

    public static void main(final String[] mainArgs) {
        TradefedSandboxRunner console = new TradefedSandboxRunner();
        console.run(mainArgs);
        System.exit(console.getErrorCode().getCodeValue());
    }

    /** A stub {@link IScheduledInvocationListener} that does nothing. */
    public static class StubScheduledInvocationListener implements IScheduledInvocationListener {
        @Override
        public void invocationComplete(
                IInvocationContext metadata, Map<ITestDevice, FreeDeviceState> devicesStates) {
            // do nothing
        }
    }

    private class DebugThread extends Thread {

        DebugThread() {
            setDaemon(true);
            setName("TradefedSandboxRunner-DebugThread");
        }

        @Override
        public void run() {
            System.out.println("Starting the DebugThread");
            try {
                File dumpStack = FileUtil.createTempFile("debugger-thread-stacks", ".txt");
                while (true) {
                    RunUtil.getDefault().sleep(300000L);
                    try (PrintStream ps = new PrintStream(dumpStack)) {
                        dumpStacks(ps);
                    }

                    printMemoryMessage();
                }
            } catch (IOException e) {
                System.err.println(e);
            }
        }
    }

    private void dumpStacks(PrintStream ps) {
        Map<Thread, StackTraceElement[]> threadMap = Thread.getAllStackTraces();
        for (Map.Entry<Thread, StackTraceElement[]> threadEntry : threadMap.entrySet()) {
            dumpThreadStack(threadEntry.getKey(), threadEntry.getValue(), ps);
        }
    }

    private void dumpThreadStack(Thread thread, StackTraceElement[] trace, PrintStream ps) {
        printLine(String.format("%s", thread), ps);
        for (int i = 0; i < trace.length; i++) {
            printLine(String.format("\t%s", trace[i]), ps);
        }
        printLine("", ps);
    }

    private void printLine(String output, PrintStream pw) {
        pw.print(output);
        pw.println();
    }

    private void printMemoryMessage() {
        long diff = Runtime.getRuntime().totalMemory() - Runtime.getRuntime().freeMemory();
        System.out.println(
                String.format(
                        "=====\nTotal: %s, Free: %s, Diff: %s\n=====",
                        Runtime.getRuntime().totalMemory(),
                        Runtime.getRuntime().freeMemory(),
                        diff));
    }
}
