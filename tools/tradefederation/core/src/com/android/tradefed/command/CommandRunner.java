/*
 * Copyright (C) 2011 The Android Open Source Project
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

package com.android.tradefed.command;

import com.android.tradefed.clearcut.ClearcutClient;
import com.android.tradefed.clearcut.TerminateClearcutClient;
import com.android.tradefed.config.ConfigurationException;
import com.android.tradefed.config.GlobalConfiguration;
import com.android.tradefed.device.NoDeviceException;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.SerializationUtil;

import com.google.common.annotations.VisibleForTesting;

import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.IOException;

/**
 * An alternate TradeFederation entry point that will run command specified in command
 * line arguments and then quit.
 * <p/>
 * Intended for use with a debugger and other non-interactive modes of operation.
 * <p/>
 * Expected arguments: [commands options] (config to run)
 */
public class CommandRunner {
    private ICommandScheduler mScheduler;
    private ExitCode mErrorCode = ExitCode.NO_ERROR;

    public static final String EXCEPTION_KEY = "serialized_exception";
    private static final long CHECK_DEVICE_TIMEOUT = 60000;

    public CommandRunner() {}

    public ExitCode getErrorCode() {
        return mErrorCode;
    }

    /**
     * Initialize the required global configuration.
     */
    @VisibleForTesting
    void initGlobalConfig(String[] args) throws ConfigurationException {
        GlobalConfiguration.createGlobalConfiguration(args);
        GlobalConfiguration.getInstance().setup();
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

    /** Returns the timeout after which to check for the command. */
    @VisibleForTesting
    long getCheckDeviceTimeout() {
        return CHECK_DEVICE_TIMEOUT;
    }

    /**
     * The main method to run the command.
     *
     * @param args the config name to run and its options
     */
    public void run(String[] args) {
        try {
            initGlobalConfig(args);

            ClearcutClient client = new ClearcutClient();
            Runtime.getRuntime().addShutdownHook(new TerminateClearcutClient(client));
            client.notifyTradefedStartEvent();

            mScheduler = getCommandScheduler();
            mScheduler.setClearcutClient(client);
            mScheduler.start();
            mScheduler.addCommand(args);
        } catch (ConfigurationException e) {
            printStackTrace(e);
            mErrorCode = ExitCode.CONFIG_EXCEPTION;
        } finally {
            mScheduler.shutdownOnEmpty();
        }
        try {
            mScheduler.join(getCheckDeviceTimeout());
            // FIXME: if possible make the no_device allocated check deterministic.
            // After 1 min we check if the command was executed.
            if (mScheduler.getReadyCommandCount() > 0
                    && mScheduler.getExecutingCommandCount() == 0) {
                printStackTrace(new NoDeviceException("No device was allocated for the command."));
                mErrorCode = ExitCode.NO_DEVICE_ALLOCATED;
                mScheduler.removeAllCommands();
                mScheduler.shutdown();
                return;
            }
            mScheduler.join();
            // If no error code has been raised yet, we checked the invocation error code.
            if (ExitCode.NO_ERROR.equals(mErrorCode)) {
                mErrorCode = mScheduler.getLastInvocationExitCode();
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
            mErrorCode = ExitCode.THROWABLE_EXCEPTION;
        } finally {
            GlobalConfiguration.getInstance().cleanup();
        }
        if (!ExitCode.NO_ERROR.equals(mErrorCode)
                && mScheduler.getLastInvocationThrowable() != null) {
            // Print error to the stderr so that it can be recovered.
            printStackTrace(mScheduler.getLastInvocationThrowable());
        }
    }

    public static void main(final String[] mainArgs) {
        CommandRunner console = new CommandRunner();
        console.run(mainArgs);
        System.exit(console.getErrorCode().getCodeValue());
    }

    /**
     * Error codes that are possible to exit with.
     */
    public static enum ExitCode {
        NO_ERROR(0),
        CONFIG_EXCEPTION(1),
        NO_BUILD(2),
        DEVICE_UNRESPONSIVE(3),
        DEVICE_UNAVAILABLE(4),
        FATAL_HOST_ERROR(5),
        THROWABLE_EXCEPTION(6),
        NO_DEVICE_ALLOCATED(7),
        WRONG_JAVA_VERSION(8);

        private final int mCodeValue;

        ExitCode(int codeValue) {
            mCodeValue = codeValue;
        }

        public int getCodeValue() {
            return mCodeValue;
        }
    }
}
