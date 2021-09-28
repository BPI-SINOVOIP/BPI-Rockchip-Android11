/*
 * Copyright (C) 2010 The Android Open Source Project
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

package com.android.tradefed.util;

import com.android.annotations.Nullable;
import com.android.tradefed.command.CommandInterrupter;
import com.android.tradefed.log.LogUtil.CLog;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Strings;

import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.ProcessBuilder.Redirect;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import javax.annotation.Nonnull;

/**
 * A collection of helper methods for executing operations.
 */
public class RunUtil implements IRunUtil {

    public static final String RUNNABLE_NOTIFIER_NAME = "RunnableNotifier";
    public static final String INHERITIO_PREFIX = "inheritio-";

    private static final int POLL_TIME_INCREASE_FACTOR = 4;
    private static final long THREAD_JOIN_POLL_INTERVAL = 30 * 1000;
    private static final long IO_THREAD_JOIN_INTERVAL = 5 * 1000;
    private static final long PROCESS_DESTROY_TIMEOUT_SEC = 2;
    private static IRunUtil sDefaultInstance = null;
    private File mWorkingDir = null;
    private Map<String, String> mEnvVariables = new HashMap<String, String>();
    private Set<String> mUnsetEnvVariables = new HashSet<String>();
    private EnvPriority mEnvVariablePriority = EnvPriority.UNSET;
    private boolean mRedirectStderr = false;

    private final CommandInterrupter mInterrupter;

    /**
     * Create a new {@link RunUtil} object to use.
     */
    public RunUtil() {
        this(CommandInterrupter.INSTANCE);
    }

    @VisibleForTesting
    RunUtil(@Nonnull CommandInterrupter interrupter) {
        mInterrupter = interrupter;
    }

    /**
     * Get a reference to the default {@link RunUtil} object.
     * <p/>
     * This is useful for callers who want to use IRunUtil without customization.
     * Its recommended that callers who do need a custom IRunUtil instance
     * (ie need to call either {@link #setEnvVariable(String, String)} or
     * {@link #setWorkingDir(File)} create their own copy.
     */
    public static IRunUtil getDefault() {
        if (sDefaultInstance == null) {
            sDefaultInstance = new RunUtil();
        }
        return sDefaultInstance;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public synchronized void setWorkingDir(File dir) {
        if (this.equals(sDefaultInstance)) {
            throw new UnsupportedOperationException("Cannot setWorkingDir on default RunUtil");
        }
        mWorkingDir = dir;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public synchronized void setEnvVariable(String name, String value) {
        if (this.equals(sDefaultInstance)) {
            throw new UnsupportedOperationException("Cannot setEnvVariable on default RunUtil");
        }
        mEnvVariables.put(name, value);
    }

    /**
     * {@inheritDoc}
     * Environment variables may inherit from the parent process, so we need to delete
     * the environment variable from {@link ProcessBuilder#environment()}
     *
     * @param key the variable name
     * @see ProcessBuilder#environment()
     */
    @Override
    public synchronized void unsetEnvVariable(String key) {
        if (this.equals(sDefaultInstance)) {
            throw new UnsupportedOperationException("Cannot unsetEnvVariable on default RunUtil");
        }
        mUnsetEnvVariables.add(key);
    }

    /** {@inheritDoc} */
    @Override
    public void setRedirectStderrToStdout(boolean redirect) {
        if (this.equals(sDefaultInstance)) {
            throw new UnsupportedOperationException(
                    "Cannot setRedirectStderrToStdout on default RunUtil");
        }
        mRedirectStderr = redirect;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public CommandResult runTimedCmd(final long timeout, final String... command) {
        return runTimedCmd(timeout, null, null, command);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public CommandResult runTimedCmd(final long timeout, OutputStream stdout,
            OutputStream stderr, final String... command) {
        RunnableResult osRunnable = createRunnableResult(stdout, stderr, command);
        CommandStatus status = runTimed(timeout, osRunnable, true);
        CommandResult result = osRunnable.getResult();
        result.setStatus(status);
        return result;
    }

    /**
     * Create a {@link com.android.tradefed.util.IRunUtil.IRunnableResult} that will run the
     * command.
     */
    @VisibleForTesting
    RunnableResult createRunnableResult(
            OutputStream stdout, OutputStream stderr, String... command) {
        return new RunnableResult(
                /* input= */ null,
                createProcessBuilder(command),
                stdout,
                stderr,
                /* inputRedirect= */ null,
                false);
    }

    /** {@inheritDoc} */
    @Override
    public CommandResult runTimedCmdRetry(
            long timeout, long retryInterval, int attempts, String... command) {
        CommandResult result = null;
        int counter = 0;
        while (counter < attempts) {
            result = runTimedCmd(timeout, command);
            if (CommandStatus.SUCCESS.equals(result.getStatus())) {
                return result;
            }
            sleep(retryInterval);
            counter++;
        }
        return result;
    }

    private synchronized ProcessBuilder createProcessBuilder(String... command) {
        return createProcessBuilder(Arrays.asList(command));
    }

    private synchronized ProcessBuilder createProcessBuilder(Redirect redirect, String... command) {
        return createProcessBuilder(redirect, Arrays.asList(command));
    }

    private synchronized ProcessBuilder createProcessBuilder(List<String> commandList) {
        return createProcessBuilder(null, commandList);
    }

    private synchronized ProcessBuilder createProcessBuilder(
            Redirect redirect, List<String> commandList) {
        ProcessBuilder processBuilder = new ProcessBuilder();
        if (mWorkingDir != null) {
            processBuilder.directory(mWorkingDir);
        }
        // By default unset an env. for process has higher priority, but in some case we might want
        // the 'set' to have priority.
        if (EnvPriority.UNSET.equals(mEnvVariablePriority)) {
            if (!mEnvVariables.isEmpty()) {
                processBuilder.environment().putAll(mEnvVariables);
            }
            if (!mUnsetEnvVariables.isEmpty()) {
                // in this implementation, the unsetEnv's priority is higher than set.
                processBuilder.environment().keySet().removeAll(mUnsetEnvVariables);
            }
        } else {
            if (!mUnsetEnvVariables.isEmpty()) {
                processBuilder.environment().keySet().removeAll(mUnsetEnvVariables);
            }
            if (!mEnvVariables.isEmpty()) {
                // in this implementation, the setEnv's priority is higher than set.
                processBuilder.environment().putAll(mEnvVariables);
            }
        }
        processBuilder.redirectErrorStream(mRedirectStderr);
        if (redirect != null) {
            processBuilder.redirectOutput(redirect);
            processBuilder.redirectError(redirect);
        }
        return processBuilder.command(commandList);
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public CommandResult runTimedCmdWithInput(final long timeout, String input,
            final String... command) {
        return runTimedCmdWithInput(timeout, input, ArrayUtil.list(command));
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public CommandResult runTimedCmdWithInput(final long timeout, String input,
            final List<String> command) {
        RunnableResult osRunnable = new RunnableResult(input, createProcessBuilder(command));
        CommandStatus status = runTimed(timeout, osRunnable, true);
        CommandResult result = osRunnable.getResult();
        result.setStatus(status);
        return result;
    }

    /** {@inheritDoc} */
    @Override
    public CommandResult runTimedCmdWithInputRedirect(
            final long timeout, @Nullable File inputRedirect, final String... command) {
        RunnableResult osRunnable =
                new RunnableResult(
                        /* input= */ null,
                        createProcessBuilder(command),
                        /* stdoutStream= */ null,
                        /* stderrStream= */ null,
                        inputRedirect,
                        true);
        CommandStatus status = runTimed(timeout, osRunnable, true);
        CommandResult result = osRunnable.getResult();
        result.setStatus(status);
        return result;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public CommandResult runTimedCmdSilently(final long timeout, final String... command) {
        RunnableResult osRunnable = new RunnableResult(null, createProcessBuilder(command), false);
        CommandStatus status = runTimed(timeout, osRunnable, false);
        CommandResult result = osRunnable.getResult();
        result.setStatus(status);
        return result;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public CommandResult runTimedCmdSilentlyRetry(long timeout, long retryInterval, int attempts,
            String... command) {
        CommandResult result = null;
        int counter = 0;
        while (counter < attempts) {
            result = runTimedCmdSilently(timeout, command);
            if (CommandStatus.SUCCESS.equals(result.getStatus())) {
                return result;
            }
            sleep(retryInterval);
            counter++;
        }
        return result;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Process runCmdInBackground(final String... command) throws IOException  {
        return runCmdInBackground(null, command);
    }

    /** {@inheritDoc} */
    @Override
    public Process runCmdInBackground(Redirect redirect, final String... command)
            throws IOException {
        final String fullCmd = Arrays.toString(command);
        CLog.v("Running in background: %s", fullCmd);
        return createProcessBuilder(redirect, command).start();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Process runCmdInBackground(final List<String> command) throws IOException  {
        return runCmdInBackground(null, command);
    }

    /** {@inheritDoc} */
    @Override
    public Process runCmdInBackground(Redirect redirect, final List<String> command)
            throws IOException {
        CLog.v("Running in background: %s", command);
        return createProcessBuilder(redirect, command).start();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public Process runCmdInBackground(List<String> command, OutputStream output)
            throws IOException {
        CLog.v("Running in background: %s", command);
        Process process = createProcessBuilder(command).start();
        inheritIO(
                process.getInputStream(),
                output,
                String.format(INHERITIO_PREFIX + "stdout-%s", command));
        inheritIO(
                process.getErrorStream(),
                output,
                String.format(INHERITIO_PREFIX + "stderr-%s", command));
        return process;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public CommandStatus runTimed(long timeout, IRunUtil.IRunnableResult runnable,
            boolean logErrors) {
        mInterrupter.checkInterrupted();
        RunnableNotifier runThread = new RunnableNotifier(runnable, logErrors);
        if (logErrors) {
            if (timeout > 0l) {
                CLog.d(
                        "Running command %s with timeout: %s",
                        runnable.getCommand(), TimeUtil.formatElapsedTime(timeout));
            } else {
                CLog.d("Running command %s without timeout.", runnable.getCommand());
            }
        }
        CommandStatus status = CommandStatus.TIMED_OUT;
        try {
            runThread.start();
            long startTime = System.currentTimeMillis();
            long pollInterval = 0;
            if (timeout > 0L && timeout < THREAD_JOIN_POLL_INTERVAL) {
                // only set the pollInterval if we have a timeout
                pollInterval = timeout;
            } else {
                pollInterval = THREAD_JOIN_POLL_INTERVAL;
            }
            do {
                try {
                    runThread.join(pollInterval);
                } catch (InterruptedException e) {
                    if (isInterruptAllowed()) {
                        CLog.i("runTimed: interrupted while joining the runnable");
                        break;
                    } else {
                        CLog.i("runTimed: currently uninterruptible, ignoring interrupt");
                    }
                }
                mInterrupter.checkInterrupted();
            } while ((timeout == 0L || (System.currentTimeMillis() - startTime) < timeout)
                    && runThread.isAlive());
        } catch (RunInterruptedException e) {
            runThread.cancel();
            throw e;
        } finally {
            // Snapshot the status when out of the run loop because thread may terminate and return
            // a false FAILED instead of TIMED_OUT.
            status = runThread.getStatus();
            if (CommandStatus.TIMED_OUT.equals(status) || CommandStatus.EXCEPTION.equals(status)) {
                CLog.i("runTimed: Calling interrupt, status is %s", status);
                runThread.cancel();
            }
        }
        mInterrupter.checkInterrupted();
        return status;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean runTimedRetry(long opTimeout, long pollInterval, int attempts,
            IRunUtil.IRunnableResult runnable) {
        for (int i = 0; i < attempts; i++) {
            if (runTimed(opTimeout, runnable, true) == CommandStatus.SUCCESS) {
                return true;
            }
            CLog.d("operation failed, waiting for %d ms", pollInterval);
            sleep(pollInterval);
        }
        return false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean runFixedTimedRetry(final long opTimeout, final long pollInterval,
            final long maxTime, final IRunUtil.IRunnableResult runnable) {
        final long initialTime = getCurrentTime();
        while (getCurrentTime() < (initialTime + maxTime)) {
            if (runTimed(opTimeout, runnable, true) == CommandStatus.SUCCESS) {
                return true;
            }
            CLog.d("operation failed, waiting for %d ms", pollInterval);
            sleep(pollInterval);
        }
        return false;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public boolean runEscalatingTimedRetry(final long opTimeout,
            final long initialPollInterval, final long maxPollInterval, final long maxTime,
            final IRunUtil.IRunnableResult runnable) {
        // wait an initial time provided
        long pollInterval = initialPollInterval;
        final long initialTime = getCurrentTime();
        while (true) {
            if (runTimed(opTimeout, runnable, true) == CommandStatus.SUCCESS) {
                return true;
            }
            long remainingTime = maxTime - (getCurrentTime() - initialTime);
            if (remainingTime <= 0) {
                CLog.d("operation is still failing after retrying for %d ms", maxTime);
                return false;
            } else if (remainingTime < pollInterval) {
                // cap pollInterval to a max of remainingTime
                pollInterval = remainingTime;
            }
            CLog.d("operation failed, waiting for %d ms", pollInterval);
            sleep(pollInterval);
            // somewhat arbitrarily, increase the poll time by a factor of 4 for each attempt,
            // up to the previously decided maximum
            pollInterval *= POLL_TIME_INCREASE_FACTOR;
            if (pollInterval > maxPollInterval) {
                pollInterval = maxPollInterval;
            }
        }
    }

    /**
     * Retrieves the current system clock time.
     * <p/>
     * Exposed so it can be mocked for unit testing
     */
    long getCurrentTime() {
        return System.currentTimeMillis();
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void sleep(long time) {
        mInterrupter.checkInterrupted();
        if (time <= 0) {
            return;
        }
        try {
            Thread.sleep(time);
        } catch (InterruptedException e) {
            // ignore
            CLog.d("sleep interrupted");
        }
        mInterrupter.checkInterrupted();
    }

    /** {@inheritDoc} */
    @Override
    public void allowInterrupt(boolean allow) {
        if (allow) {
            mInterrupter.allowInterrupt();
        } else {
            mInterrupter.blockInterrupt();
        }
    }

    /** {@inheritDoc} */
    @Override
    public boolean isInterruptAllowed() {
        return mInterrupter.isInterruptible();
    }

    /** {@inheritDoc} */
    @Override
    public void setInterruptibleInFuture(Thread thread, final long timeMs) {
        mInterrupter.allowInterruptAsync(thread, timeMs, TimeUnit.MILLISECONDS);
    }

    /** {@inheritDoc} */
    @Override
    public synchronized void interrupt(Thread thread, String message) {
        mInterrupter.interrupt(thread, message);
        mInterrupter.checkInterrupted();
    }

    /**
     * Helper thread that wraps a runnable, and notifies when done.
     */
    private static class RunnableNotifier extends Thread {

        private final IRunUtil.IRunnableResult mRunnable;
        private CommandStatus mStatus = CommandStatus.TIMED_OUT;
        private boolean mLogErrors = true;

        RunnableNotifier(IRunUtil.IRunnableResult runnable, boolean logErrors) {
            // Set this thread to be a daemon so that it does not prevent
            // TF from shutting down.
            setName(RUNNABLE_NOTIFIER_NAME);
            setDaemon(true);
            mRunnable = runnable;
            mLogErrors = logErrors;
        }

        @Override
        public void run() {
            CommandStatus status;
            try {
                status = mRunnable.run() ? CommandStatus.SUCCESS : CommandStatus.FAILED;
            } catch (InterruptedException e) {
                CLog.i("runutil interrupted");
                status = CommandStatus.EXCEPTION;
                backFillException(mRunnable.getResult(), e);
            } catch (Exception e) {
                if (mLogErrors) {
                    CLog.e("Exception occurred when executing runnable");
                    CLog.e(e);
                }
                status = CommandStatus.EXCEPTION;
                backFillException(mRunnable.getResult(), e);
            }
            synchronized (this) {
                mStatus = status;
            }
        }

        public void cancel() {
            mRunnable.cancel();
        }

        synchronized CommandStatus getStatus() {
            return mStatus;
        }

        private void backFillException(CommandResult result, Exception e) {
            if (result == null) {
                return;
            }
            if (Strings.isNullOrEmpty(result.getStderr())) {
                result.setStderr(StreamUtil.getStackTrace(e));
            }
        }
    }

    class RunnableResult implements IRunUtil.IRunnableResult {
        private final ProcessBuilder mProcessBuilder;
        private final CommandResult mCommandResult;
        private final String mInput;
        private Process mProcess = null;
        private CountDownLatch mCountDown = null;
        private Thread mExecutionThread;
        private OutputStream mStdOut = null;
        private OutputStream mStdErr = null;
        private final File mInputRedirect;
        private boolean mCreatedStdoutStream = false;
        private boolean mCreatedStderrStream = false;
        private final Object mLock = new Object();
        private boolean mCancelled = false;
        private boolean mLogErrors = true;

        RunnableResult(final String input, final ProcessBuilder processBuilder) {
            this(input, processBuilder, null, null, null, true);
        }

        RunnableResult(final String input, final ProcessBuilder processBuilder, boolean logErrors) {
            this(input, processBuilder, null, null, null, logErrors);
        }

        /**
         * Alternative constructor that allows redirecting the output to any Outputstream. Stdout
         * and stderr can be independently redirected to different Outputstream implementations. If
         * streams are null, default behavior of using a buffer will be used.
         *
         * <p>Additionally, Stdin can be redirected from a File.
         */
        RunnableResult(
                final String input,
                final ProcessBuilder processBuilder,
                OutputStream stdoutStream,
                OutputStream stderrStream,
                File inputRedirect,
                boolean logErrors) {
            mProcessBuilder = processBuilder;
            mInput = input;
            mLogErrors = logErrors;

            mInputRedirect = inputRedirect;
            if (mInputRedirect != null) {
                // Set Stdin to mInputRedirect file.
                mProcessBuilder.redirectInput(mInputRedirect);
            }

            mCommandResult = new CommandResult();
            // Ensure the outputs are never null
            mCommandResult.setStdout("");
            mCommandResult.setStderr("");
            mCountDown = new CountDownLatch(1);

            // Redirect IO, so that the outputstream for the spawn process does not fill up
            // and cause deadlock.
            mStdOut = stdoutStream != null ? stdoutStream : new ByteArrayOutputStream();
            mStdErr = stderrStream != null ? stderrStream : new ByteArrayOutputStream();
        }

        @Override
        public List<String> getCommand() {
            return new ArrayList<>(mProcessBuilder.command());
        }

        @Override
        public CommandResult getResult() {
            return mCommandResult;
        }

        /** Start a {@link Process} based on the {@link ProcessBuilder}. */
        @VisibleForTesting
        Process startProcess() throws IOException {
            return mProcessBuilder.start();
        }

        @Override
        public boolean run() throws Exception {
            Thread stdoutThread = null;
            Thread stderrThread = null;
            synchronized (mLock) {
                if (mCancelled) {
                    // if cancel() was called before run() took the lock, we do not even attempt
                    // to run.
                    return false;
                }
                mExecutionThread = Thread.currentThread();
                mProcess = startProcess();
                if (mInput != null) {
                    BufferedOutputStream processStdin =
                            new BufferedOutputStream(mProcess.getOutputStream());
                    processStdin.write(mInput.getBytes("UTF-8"));
                    processStdin.flush();
                    processStdin.close();
                }
                // Log the command for thread tracking purpose.
                stdoutThread =
                        inheritIO(
                                mProcess.getInputStream(),
                                mStdOut,
                                String.format("inheritio-stdout-%s", mProcessBuilder.command()));
                stderrThread =
                        inheritIO(
                                mProcess.getErrorStream(),
                                mStdErr,
                                String.format("inheritio-stderr-%s", mProcessBuilder.command()));

                // Close the stdout/err streams if created by us. Streams provided by the caller
                // should be closed by the caller.
                if (mCreatedStdoutStream) {
                    mStdOut.close();
                }
                if (mCreatedStderrStream) {
                    mStdErr.close();
                }
            }
            // Wait for process to complete.
            Integer rc = null;
            try {
                try {
                    rc = mProcess.waitFor();
                    // wait for stdout and stderr to be read
                    if (stdoutThread != null) {
                        stdoutThread.join(IO_THREAD_JOIN_INTERVAL);
                        if (stdoutThread.isAlive()) {
                            CLog.d("stdout read thread %s still alive.", stdoutThread.toString());
                        }
                    }
                    if (stderrThread != null) {
                        stderrThread.join(IO_THREAD_JOIN_INTERVAL);
                        if (stderrThread.isAlive()) {
                            CLog.d("stderr read thread %s still alive.", stderrThread.toString());
                        }
                    }
                } finally {
                    rc = (rc != null) ? rc : 1; // In case of interruption ReturnCode is null
                    mCommandResult.setExitCode(rc);

                    // Write out the streams to the result.
                    if (mStdOut instanceof ByteArrayOutputStream) {
                        mCommandResult.setStdout(
                                ((ByteArrayOutputStream) mStdOut).toString("UTF-8"));
                    } else {
                        mCommandResult.setStdout(
                                "redirected to " + mStdOut.getClass().getSimpleName());
                    }
                    if (mStdErr instanceof ByteArrayOutputStream) {
                        mCommandResult.setStderr(
                                ((ByteArrayOutputStream) mStdErr).toString("UTF-8"));
                    } else {
                        mCommandResult.setStderr(
                                "redirected to " + mStdErr.getClass().getSimpleName());
                    }
                }
            } finally {
                mCountDown.countDown();
            }

            if (rc != null && rc == 0) {
                return true;
            } else if (mLogErrors) {
                CLog.d("%s command failed. return code %d", mProcessBuilder.command(), rc);
            }
            return false;
        }

        @Override
        public void cancel() {
            if (mCancelled) {
                return;
            }
            mCancelled = true;
            synchronized (mLock) {
                if (mProcess == null || !mProcess.isAlive()) {
                    return;
                }
                CLog.d("Cancelling the process execution");
                mProcess.destroy();
                try {
                    // Only allow to continue if the Stdout has been read
                    // RunnableNotifier#Interrupt is the next call and will terminate the thread
                    if (!mCountDown.await(PROCESS_DESTROY_TIMEOUT_SEC, TimeUnit.SECONDS)) {
                        CLog.i("Process still not terminated, interrupting the execution thread");
                        mExecutionThread.interrupt();
                        mCountDown.await();
                    }
                } catch (InterruptedException e) {
                    CLog.i("interrupted while waiting for process output to be saved");
                }
            }
        }

        @Override
        public String toString() {
            return "RunnableResult [command="
                    + ((mProcessBuilder != null) ? mProcessBuilder.command() : null)
                    + "]";
        }
    }

    /**
     * Helper method to redirect input stream.
     *
     * @param src {@link InputStream} to inherit/redirect from
     * @param dest {@link BufferedOutputStream} to inherit/redirect to
     * @param name the name of the thread returned.
     * @return a {@link Thread} started that receives the IO.
     */
    private static Thread inheritIO(final InputStream src, final OutputStream dest, String name) {
        // In case of some Process redirect, source stream can be null.
        if (src == null) {
            return null;
        }
        Thread t =
                new Thread(
                        new Runnable() {
                            @Override
                            public void run() {
                                try {
                                    StreamUtil.copyStreams(src, dest);
                                } catch (IOException e) {
                                    CLog.e("Failed to read input stream %s.", name);
                                }
                            }
                        });
        t.setName(name);
        t.start();
        return t;
    }

    /** {@inheritDoc} */
    @Override
    public void setEnvVariablePriority(EnvPriority priority) {
        if (this.equals(sDefaultInstance)) {
            throw new UnsupportedOperationException("Cannot setWorkingDir on default RunUtil");
        }
        mEnvVariablePriority = priority;
    }
}
