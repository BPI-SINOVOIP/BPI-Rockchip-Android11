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
package com.android.tradefed.config;

import com.android.annotations.VisibleForTesting;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.config.OptionSetter.OptionFieldsForName;
import com.android.tradefed.config.remote.IRemoteFileResolver;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.logger.CurrentInvocation;
import com.android.tradefed.invoker.logger.CurrentInvocation.InvocationInfo;
import com.android.tradefed.invoker.logger.InvocationLocal;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.util.FileUtil;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.ZipUtil;
import com.android.tradefed.util.ZipUtil2;

import com.google.common.collect.ImmutableMap;
import com.google.common.collect.Maps;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Field;
import java.net.URI;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.ServiceLoader;
import java.util.Set;
import java.util.function.Supplier;

import javax.annotation.Nullable;
import javax.annotation.concurrent.GuardedBy;
import javax.annotation.concurrent.ThreadSafe;

/**
 * Class that helps resolving path to remote files.
 *
 * <p>For example: gs://bucket/path/file.txt will be resolved by downloading the file from the GCS
 * bucket.
 *
 * <p>New protocols should be added to META_INF/services.
 */
public class DynamicRemoteFileResolver {

    // Query key for requesting to unzip a downloaded file automatically.
    public static final String UNZIP_KEY = "unzip";
    // Query key for requesting a download to be optional, so if it fails we don't replace it.
    public static final String OPTIONAL_KEY = "optional";

    /**
     * Loads file resolvers using a dedicated {@link ServiceFileResolverLoader} that is scoped to
     * each invocation.
     */
    // TODO(hzalek): Store a DynamicRemoteFileResolver instance per invocation to avoid locals.
    private static final FileResolverLoader DEFAULT_FILE_RESOLVER_LOADER =
            new FileResolverLoader() {
                private final InvocationLocal<FileResolverLoader> mInvocationLoader =
                        new InvocationLocal<FileResolverLoader>() {
                            @Override
                            protected FileResolverLoader initialValue() {
                                return new ServiceFileResolverLoader();
                            }
                        };

                @Override
                public IRemoteFileResolver load(String scheme, Map<String, String> config) {
                    return mInvocationLoader.get().load(scheme, config);
                }
            };

    private final FileResolverLoader mFileResolverLoader;

    private Map<String, OptionFieldsForName> mOptionMap;
    // Populated from {@link ICommandOptions#getDynamicDownloadArgs()}
    private Map<String, String> mExtraArgs = new LinkedHashMap<>();
    private ITestDevice mDevice;

    public DynamicRemoteFileResolver() {
        this(DEFAULT_FILE_RESOLVER_LOADER);
    }

    @VisibleForTesting
    public DynamicRemoteFileResolver(FileResolverLoader loader) {
        this.mFileResolverLoader = loader;
    }

    /** Sets the map of options coming from {@link OptionSetter} */
    public void setOptionMap(Map<String, OptionFieldsForName> optionMap) {
        mOptionMap = optionMap;
    }

    /** Sets the device under tests */
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    /** Add extra args for the query. */
    public void addExtraArgs(Map<String, String> extraArgs) {
        mExtraArgs.putAll(extraArgs);
    }

    /**
     * Runs through all the {@link File} option type and check if their path should be resolved.
     *
     * @return The list of {@link File} that was resolved that way.
     * @throws BuildRetrievalError
     */
    public final Set<File> validateRemoteFilePath() throws BuildRetrievalError {
        Set<File> downloadedFiles = new HashSet<>();
        try {
            Map<Field, Object> fieldSeen = new HashMap<>();
            for (Map.Entry<String, OptionFieldsForName> optionPair : mOptionMap.entrySet()) {
                final OptionFieldsForName optionFields = optionPair.getValue();
                for (Map.Entry<Object, Field> fieldEntry : optionFields) {

                    final Object obj = fieldEntry.getKey();
                    final Field field = fieldEntry.getValue();
                    final Option option = field.getAnnotation(Option.class);
                    if (option == null) {
                        continue;
                    }
                    // At this point, we know this is an option field; make sure it's set
                    field.setAccessible(true);
                    final Object value;
                    try {
                        value = field.get(obj);
                        if (value == null) {
                            continue;
                        }
                    } catch (IllegalAccessException e) {
                        throw new BuildRetrievalError(
                                String.format("internal error: %s", e.getMessage()),
                                InfraErrorIdentifier.ARTIFACT_UNSUPPORTED_PATH);
                    }

                    if (fieldSeen.get(field) != null && fieldSeen.get(field).equals(obj)) {
                        continue;
                    }
                    // Keep track of the field set on each object
                    fieldSeen.put(field, obj);

                    // The below contains unchecked casts that are mostly safe because we add/remove
                    // items of a type already in the collection; assuming they're not instances of
                    // some subclass of File. This is unlikely since we populate the items during
                    // option injection. The possibility still exists that constructors of
                    // initialized objects add objects that are instances of a File subclass. A
                    // safer approach would be to have a custom type that can be deferenced to
                    // access the resolved target file. This would also have the benefit of not
                    // having to modify any user collections and preserve the ordering.

                    if (value instanceof File) {
                        File consideredFile = (File) value;
                        File downloadedFile = resolveRemoteFiles(consideredFile, option);
                        if (downloadedFile != null) {
                            downloadedFiles.add(downloadedFile);
                            // Replace the field value
                            try {
                                field.set(obj, downloadedFile);
                            } catch (IllegalAccessException e) {
                                CLog.e(e);
                                throw new BuildRetrievalError(
                                        String.format(
                                                "Failed to download %s due to '%s'",
                                                consideredFile.getPath(), e.getMessage()),
                                        e);
                            }
                        }
                    } else if (value instanceof Collection) {
                        @SuppressWarnings("unchecked")  // Mostly-safe, see above comment.
                        Collection<Object> c = (Collection<Object>) value;
                        Collection<Object> copy = new ArrayList<>(c);
                        for (Object o : copy) {
                            if (o instanceof File) {
                                File consideredFile = (File) o;
                                File downloadedFile = resolveRemoteFiles(consideredFile, option);
                                if (downloadedFile != null) {
                                    downloadedFiles.add(downloadedFile);
                                    // TODO: See if order could be preserved.
                                    c.remove(consideredFile);
                                    c.add(downloadedFile);
                                }
                            }
                        }
                    } else if (value instanceof Map) {
                        @SuppressWarnings("unchecked")  // Mostly-safe, see above comment.
                        Map<Object, Object> m = (Map<Object, Object>) value;
                        Map<Object, Object> copy = new LinkedHashMap<>(m);
                        for (Entry<Object, Object> entry : copy.entrySet()) {
                            Object key = entry.getKey();
                            Object val = entry.getValue();

                            Object finalKey = key;
                            Object finalVal = val;
                            if (key instanceof File) {
                                key = resolveRemoteFiles((File) key, option);
                                if (key != null) {
                                    downloadedFiles.add((File) key);
                                    finalKey = key;
                                }
                            }
                            if (val instanceof File) {
                                val = resolveRemoteFiles((File) val, option);
                                if (val != null) {
                                    downloadedFiles.add((File) val);
                                    finalVal = val;
                                }
                            }

                            m.remove(entry.getKey());
                            m.put(finalKey, finalVal);
                        }
                    } else if (value instanceof MultiMap) {
                        @SuppressWarnings("unchecked")  // Mostly-safe, see above comment.
                        MultiMap<Object, Object> m = (MultiMap<Object, Object>) value;
                        synchronized (m) {
                            MultiMap<Object, Object> copy = new MultiMap<>(m);
                            for (Object key : copy.keySet()) {
                                List<Object> mapValues = copy.get(key);

                                m.remove(key);
                                Object finalKey = key;
                                if (key instanceof File) {
                                    key = resolveRemoteFiles((File) key, option);
                                    if (key != null) {
                                        downloadedFiles.add((File) key);
                                        finalKey = key;
                                    }
                                }
                                for (Object mapValue : mapValues) {
                                    if (mapValue instanceof File) {
                                        File f = resolveRemoteFiles((File) mapValue, option);
                                        if (f != null) {
                                            downloadedFiles.add(f);
                                            mapValue = f;
                                        }
                                    }
                                    m.put(finalKey, mapValue);
                                }
                            }
                        }
                    }
                }
            }
        } catch (RuntimeException | BuildRetrievalError e) {
            // Clean up the files before throwing
            for (File f : downloadedFiles) {
                FileUtil.recursiveDelete(f);
            }
            throw e;
        }
        return downloadedFiles;
    }

    /**
     * Download the files matching given filters in a remote zip file.
     *
     * <p>A file inside the remote zip file is only downloaded if its path matches any of the
     * include filters but not the exclude filters.
     *
     * @param destDir the file to place the downloaded contents into.
     * @param remoteZipFilePath the remote path to the zip file to download, relative to an
     *     implementation specific root.
     * @param includeFilters a list of regex strings to download matching files. A file's path
     *     matching any filter will be downloaded.
     * @param excludeFilters a list of regex strings to skip downloading matching files. A file's
     *     path matching any filter will not be downloaded.
     * @throws BuildRetrievalError if files could not be downloaded.
     */
    public void resolvePartialDownloadZip(
            File destDir,
            String remoteZipFilePath,
            List<String> includeFilters,
            List<String> excludeFilters)
            throws BuildRetrievalError {
        Map<String, String> queryArgs;
        String protocol;
        try {
            URI uri = new URI(remoteZipFilePath);
            protocol = uri.getScheme();
            queryArgs = parseQuery(uri.getQuery());
        } catch (URISyntaxException e) {
            throw new BuildRetrievalError(
                    String.format(
                            "Failed to parse the remote zip file path: %s", remoteZipFilePath),
                    e);
        }
        queryArgs.put("partial_download_dir", destDir.getAbsolutePath());
        if (includeFilters != null) {
            queryArgs.put("include_filters", String.join(";", includeFilters));
        }
        if (excludeFilters != null) {
            queryArgs.put("exclude_filters", String.join(";", excludeFilters));
        }
        // Downloaded individual files should be saved to destDir, return value is not needed.
        try {
            IRemoteFileResolver resolver = getResolver(protocol);
            resolver.setPrimaryDevice(mDevice);
            resolver.resolveRemoteFiles(new File(remoteZipFilePath), queryArgs);
        } catch (BuildRetrievalError e) {
            if (isOptional(queryArgs)) {
                CLog.d(
                        "Failed to partially download '%s' but marked optional so skipping: %s",
                        remoteZipFilePath, e.getMessage());
                return;
            }

            throw e;
        }
    }

    private IRemoteFileResolver getResolver(String protocol) throws BuildRetrievalError {
        try {
        return mFileResolverLoader.load(protocol, mExtraArgs);
        } catch (ResolverLoadingException e) {
            throw new BuildRetrievalError(
                    String.format("Could not load resolver for protocol %s", protocol), e);
        }
    }

    @VisibleForTesting
    IGlobalConfiguration getGlobalConfig() {
        return GlobalConfiguration.getInstance();
    }

    /**
     * Utility that allows to check whether or not a file should be unzip and unzip it if required.
     */
    public static final File unzipIfRequired(File downloadedFile, Map<String, String> query)
            throws IOException {
        String unzipValue = query.get(UNZIP_KEY);
        if (unzipValue != null && "true".equals(unzipValue.toLowerCase())) {
            // File was requested to be unzipped.
            if (ZipUtil.isZipFileValid(downloadedFile, false)) {
                File extractedDir =
                        FileUtil.createTempDir(
                                FileUtil.getBaseName(downloadedFile.getName()),
                                CurrentInvocation.getInfo(InvocationInfo.WORK_FOLDER));
                ZipUtil2.extractZip(downloadedFile, extractedDir);
                FileUtil.deleteFile(downloadedFile);
                return extractedDir;
            } else {
                CLog.w("%s was requested to be unzipped but is not a valid zip.", downloadedFile);
            }
        }
        // Return the original file untouched
        return downloadedFile;
    }

    private File resolveRemoteFiles(File consideredFile, Option option) throws BuildRetrievalError {
        File fileToResolve;
        String path = consideredFile.getPath();
        String protocol;
        Map<String, String> query;
        try {
            URI uri = new URI(path);
            protocol = uri.getScheme();
            query = parseQuery(uri.getQuery());
            fileToResolve = new File(protocol + ":" + uri.getPath());
        } catch (URISyntaxException e) {
            CLog.e(e);
            return null;
        }

        try {
            IRemoteFileResolver resolver = getResolver(protocol);
            if (resolver == null) {
                return null;
            }

            CLog.d("Considering option '%s' with path: '%s' for download.", option.name(), path);
            resolver.setPrimaryDevice(mDevice);
            return resolver.resolveRemoteFiles(fileToResolve, query);
        } catch (BuildRetrievalError e) {
            if (isOptional(query)) {
                CLog.d(
                        "Failed to resolve '%s' but marked optional so skipping: %s",
                        fileToResolve, e.getMessage());
                return null;
            }

            throw e;
        }
    }

    /**
     * Parse a URL query style. Delimited by &, and map values represented by =. Example:
     * ?key=value&key2=value2
     */
    private Map<String, String> parseQuery(String query) {
        Map<String, String> values = new HashMap<>();
        if (query == null) {
            return values;
        }
        for (String maps : query.split("&")) {
            String[] keyVal = maps.split("=");
            values.put(keyVal[0], keyVal[1]);
        }
        return values;
    }

    /** Whether or not a link was requested as optional. */
    private boolean isOptional(Map<String, String> query) {
        String value = query.get(OPTIONAL_KEY);
        if (value == null) {
            return false;
        }
        return "true".equals(value.toLowerCase());
    }

    /** Loads implementations of {@link IRemoteFileResolver}. */
    @VisibleForTesting
    public interface FileResolverLoader {
        /**
         * Loads a resolver that can handle the provided scheme.
         *
         * @param scheme the URI scheme that the loaded resolver is expected to handle.
         * @param config a map of all dynamic resolver configuration key-value pairs specified by
         *     the 'dynamic-resolver-args' TF command-line flag.
         * @throws ResolverLoadingException if the resolver that handles the specified scheme cannot
         *     be loaded and/or initialized.
         */
        @Nullable
        IRemoteFileResolver load(String scheme, Map<String, String> config);
    }

    /** Exception thrown if a resolver cannot be loaded or initialized. */
    @VisibleForTesting
    static final class ResolverLoadingException extends RuntimeException {
        public ResolverLoadingException(@Nullable String message) {
            super(message);
        }

        public ResolverLoadingException(@Nullable Throwable cause) {
            super(cause);
        }

        public ResolverLoadingException(@Nullable String message, @Nullable Throwable cause) {
            super(message, cause);
        }
    }

    /**
     * Loads and caches file resolvers using the service loading facility.
     *
     * <p>This implementation uses the service loading facility to find and cache available
     * resolvers on the first call to {@code load}.
     *
     * <p>Any {@link Option}-annotated fields defined in loaded resolvers are initialized from the
     * provided key-value pairs using the standard TF option-setting mechanism. Resolvers can define
     * options that themselves require resolution as long as it causes no cycles during
     * initialization.
     *
     * <p>Resolvers are loaded eagerly using ServiceLoader but have their options initialized only
     * when first used. This avoids exceptions due to missing options in resolvers that are
     * available on the class path but never used to load any files.
     *
     * <p>This implementation is thread-safe and ensures that any loaded resolvers are loaded at
     * most once per instance.
     */
    @ThreadSafe
    @VisibleForTesting
    static final class ServiceFileResolverLoader implements FileResolverLoader {
        // We need the indirection since in production we use the context class loader that is
        // defined when loading and not the one at construction.
        private final Supplier<ClassLoader> mClassLoaderSupplier;

        @GuardedBy("this")
        private @Nullable LoaderState mLoaderState;

        ServiceFileResolverLoader() {
            mClassLoaderSupplier = () -> Thread.currentThread().getContextClassLoader();
        }

        ServiceFileResolverLoader(ClassLoader classLoader) {
            mClassLoaderSupplier = () -> classLoader;
        }

        @Override
        public synchronized IRemoteFileResolver load(String scheme, Map<String, String> config) {
            if (mLoaderState != null) {
                return mLoaderState.getAndInit(scheme);
            }

            // We use an intermediate map because the ImmutableMap builder throws if we add multiple
            // entries with the same key. Note that we don't worry about setting any state that
            // prevents this code from re-executing since failures loading service providers throws
            // an Error which bubbles all the way to the top.
            Map<String, IRemoteFileResolver> resolvers = new HashMap<>();
            ServiceLoader<IRemoteFileResolver> serviceLoader =
                    ServiceLoader.load(IRemoteFileResolver.class, mClassLoaderSupplier.get());

            for (IRemoteFileResolver resolver : serviceLoader) {
                resolvers.putIfAbsent(resolver.getSupportedProtocol(), resolver);
            }

            mLoaderState = new LoaderState(resolvers, config);
            return mLoaderState.getAndInit(scheme);
        }

        /** Stores the state of loaded file resolvers. */
        private static final class LoaderState {
            private final ImmutableMap<String, String> mConfig;
            private final ImmutableMap<String, ResolverState> mState;

            LoaderState(Map<String, IRemoteFileResolver> resolvers, Map<String, String> config) {
                this.mState =
                        ImmutableMap.copyOf(
                                Maps.transformValues(resolvers, r -> new ResolverState(r)));
                this.mConfig = ImmutableMap.copyOf(config);
            }

            /** Returns an initialized resolver instance for the specified scheme. */
            @Nullable
            IRemoteFileResolver getAndInit(String scheme) {
                ResolverState state = mState.get(scheme);
                if (state == null) {
                    return null;
                }

                return state.getAndInit(this);
            }

            void resolve(IRemoteFileResolver resolver)
                    throws ConfigurationException, BuildRetrievalError {
                // The device isn't set when resolving dynamic options because we don't want to load
                // device-specific configuration when initializing pseudo-static resolvers that
                // could out-live a particular device.
                OptionSetter setter = new OptionSetter(resolver);

                for (Map.Entry<String, String> e : mConfig.entrySet()) {
                    String name = e.getKey();

                    // Note that we don't throw for options that don't exist.
                    if (setter.fieldsForArgNoThrow(name) == null) {
                        // TODO(hzalek): Consider throwing when the option doesn't exist and is
                        // qualified using one of the option source's aliases.
                        // option name uses one of
                        // the option source's aliases
                        continue;
                    }

                    if (setter.isMapOption(name)) {
                        throw new ConfigurationException("Map options are not supported: " + name);
                    }

                    setter.setOptionValue(name, e.getValue());
                }

                Collection<String> missingOptions = setter.getUnsetMandatoryOptions();
                if (!missingOptions.isEmpty()) {
                    throw new ConfigurationException(
                            String.format(
                                    "Found missing mandatory options %s for resolver %s",
                                    missingOptions, resolver.toString()));
                }

                DynamicRemoteFileResolver dynamicResolver =
                        new DynamicRemoteFileResolver((scheme, unused) -> getAndInit(scheme));
                dynamicResolver.addExtraArgs(mConfig);
                setter.validateRemoteFilePath(dynamicResolver);
            }

            /** Stores the resolver and its initialization state. */
            static final class ResolverState {
                final IRemoteFileResolver mResolver;

                /**
                 * The initialization state where {@code null} means never initialized, {@code
                 * false} means started, and {@code true} means done.
                 */
                @Nullable Boolean mDone;

                /**
                 * The exception thrown when initializing the resolver to ensure that we only do it
                 * once.
                 */
                @Nullable ResolverLoadingException mException;

                ResolverState(IRemoteFileResolver resolver) {
                    this.mResolver = resolver;
                }

                IRemoteFileResolver getAndInit(LoaderState context) {
                    if (Boolean.TRUE.equals(mDone)) {
                        return getOrThrow();
                    }

                    if (Boolean.FALSE.equals(mDone)) {
                        // No need to catch or store the exception since it gets thrown in the
                        // recursive
                        // call to the dynamic resolver as a BuildRetrievalError which we already
                        // catch.
                        throw new ResolverLoadingException(
                                "Cycle detected while initializing resolver options: "
                                        + mResolver.toString());
                    }

                    CLog.i("Initializing file resolver options: %s", mResolver);
                    mDone = Boolean.FALSE;

                    try {
                        context.resolve(mResolver);
                    } catch (BuildRetrievalError | ConfigurationException e) {
                        mException =
                                new ResolverLoadingException(
                                        "Could not initialize resolver options: "
                                                + mResolver.toString(),
                                        e);
                        throw mException;
                    } finally {
                        mDone = Boolean.TRUE;
                    }

                    return mResolver;
                }

                private IRemoteFileResolver getOrThrow() {
                    if (mException != null) {
                        throw mException;
                    }
                    return mResolver;
                }
            }
        }
    }
}
