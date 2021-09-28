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

import static com.google.common.truth.Truth.assertThat;
import com.google.common.truth.Correspondence;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import com.android.tradefed.build.BootstrapBuildProvider;
import com.android.tradefed.build.BuildRetrievalError;
import com.android.tradefed.build.StubBuildProvider;
import com.android.tradefed.config.DynamicRemoteFileResolver.FileResolverLoader;
import com.android.tradefed.config.DynamicRemoteFileResolver.ResolverLoadingException;
import com.android.tradefed.config.DynamicRemoteFileResolver.ServiceFileResolverLoader;
import com.android.tradefed.config.remote.GcsRemoteFileResolver;
import com.android.tradefed.config.remote.IRemoteFileResolver;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.shard.ParentShardReplicate;
import com.android.tradefed.result.error.InfraErrorIdentifier;
import com.android.tradefed.util.MultiMap;
import com.android.tradefed.util.RunUtil;
import com.android.tradefed.util.executor.ParallelDeviceExecutor;

import com.google.common.collect.ImmutableMap;

import org.easymock.EasyMock;
import org.easymock.IAnswer;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.Mockito;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.ServiceLoader;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.jar.JarEntry;
import java.util.jar.JarOutputStream;

/** Unit tests for {@link DynamicRemoteFileResolver}. */
@RunWith(JUnit4.class)
public class DynamicRemoteFileResolverTest {

    @Rule public final TemporaryFolder temporaryFolder = new TemporaryFolder();

    @OptionClass(alias = "alias-remote-file")
    private static class RemoteFileOption {
        @Option(name = "remote-file")
        public File remoteFile = null;

        @Option(name = "remote-file-list")
        public Collection<File> remoteFileList = new ArrayList<>();

        @Option(name = "remote-map")
        public Map<File, File> remoteMap = new HashMap<>();

        @Option(name = "remote-multi-map")
        public MultiMap<File, File> remoteMultiMap = new MultiMap<>();
    }

    @OptionClass(alias = "option-class-alias", global_namespace = false)
    private static class RemoteFileOptionWithOptionClass {
        @Option(name = "remote-file")
        public File remoteFile = null;
    }

    private DynamicRemoteFileResolver mResolver;
    private IRemoteFileResolver mMockResolver;

    @Before
    public void setUp() {
        mMockResolver = EasyMock.createNiceMock(IRemoteFileResolver.class);
        FileResolverLoader resolverLoader =
                new FileResolverLoader() {
                    @Override
                    public IRemoteFileResolver load(String scheme, Map<String, String> config) {
                        return ImmutableMap.of(GcsRemoteFileResolver.PROTOCOL, mMockResolver)
                                .get(scheme);
                    }
                };
        mResolver = new DynamicRemoteFileResolver(resolverLoader);
    }

    @Test
    public void testResolve() throws Exception {
        RemoteFileOption object = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object);
        setter.setOptionValue("remote-file", "gs://fake/path");
        File fake = temporaryFolder.newFile();
        assertEquals("gs:/fake/path", object.remoteFile.getPath());
        mMockResolver.setPrimaryDevice(null);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")),
                                EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.replay(mMockResolver);

        Set<File> downloadedFile = setter.validateRemoteFilePath(mResolver);

        // The file has been replaced by the downloaded one.
        assertThat(object.remoteFile.getAbsolutePath()).isEqualTo(fake.getAbsolutePath());
        assertThat(downloadedFile)
                .comparingElementsUsing(FILE_PATH_EQUIVALENCE)
                .containsExactly(fake);
        EasyMock.verify(mMockResolver);
    }

    @Test
    public void testResolveWithQuery() throws Exception {
        RemoteFileOption object = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object);

        File fake = temporaryFolder.newFile();

        setter.setOptionValue("remote-file", "gs://fake/path?key=value");
        assertEquals("gs:/fake/path?key=value", object.remoteFile.getPath());

        Map<String, String> testMap = new HashMap<>();
        testMap.put("key", "value");
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")),
                                EasyMock.eq(testMap)))
                .andReturn(fake);
        EasyMock.replay(mMockResolver);

        Set<File> downloadedFile = setter.validateRemoteFilePath(mResolver);
        assertEquals(1, downloadedFile.size());
        File downloaded = downloadedFile.iterator().next();
        // The file has been replaced by the downloaded one.
        assertEquals(downloaded.getAbsolutePath(), object.remoteFile.getAbsolutePath());
        EasyMock.verify(mMockResolver);
    }

    /** Test to make sure that a dynamic download marked as "optional" does not throw */
    @Test
    public void testResolveOptional() throws Exception {
        RemoteFileOption object = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object);

        setter.setOptionValue("remote-file", "gs://fake/path?optional=true");
        assertEquals("gs:/fake/path?optional=true", object.remoteFile.getPath());

        Map<String, String> testMap = new HashMap<>();
        testMap.put("optional", "true");
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")), EasyMock.eq(testMap)))
                .andThrow(
                        new BuildRetrievalError(
                                "Failed to download",
                                InfraErrorIdentifier.ARTIFACT_DOWNLOAD_ERROR));
        EasyMock.replay(mMockResolver);

        Set<File> downloadedFile = setter.validateRemoteFilePath(mResolver);
        assertEquals(0, downloadedFile.size());
        EasyMock.verify(mMockResolver);
    }

    @Test
    public void testResolve_remoteFileList() throws Exception {
        RemoteFileOption object = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object);

        File fake = temporaryFolder.newFile();

        setter.setOptionValue("remote-file-list", "gs://fake/path");
        setter.setOptionValue("remote-file-list", "fake/file");
        assertEquals(2, object.remoteFileList.size());

        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")),
                                EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.replay(mMockResolver);

        Set<File> downloadedFile = setter.validateRemoteFilePath(mResolver);
        assertEquals(1, downloadedFile.size());
        File downloaded = downloadedFile.iterator().next();
        // The file has been replaced by the downloaded one.
        assertEquals(2, object.remoteFileList.size());
        Iterator<File> ite = object.remoteFileList.iterator();
        File notGsFile = ite.next();
        assertEquals("fake/file", notGsFile.getPath());
        File gsFile = ite.next();
        assertEquals(downloaded.getAbsolutePath(), gsFile.getAbsolutePath());
        EasyMock.verify(mMockResolver);
    }

    @Test
    public void testResolve_remoteFileList_downloadError() throws Exception {
        RemoteFileOption object = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object);
        setter.setOptionValue("remote-file-list", "fake/file");
        setter.setOptionValue("remote-file-list", "gs://success/fake/path");
        setter.setOptionValue("remote-file-list", "gs://success/fake/path2");
        setter.setOptionValue("remote-file-list", "gs://failure/test");
        assertEquals(4, object.remoteFileList.size());

        File fake = temporaryFolder.newFile();
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs://success/fake/path")),
                                EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs://success/fake/path2")),
                                EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs://failure/test")), EasyMock.anyObject()))
                .andThrow(
                        new BuildRetrievalError(
                                "retrieval error", InfraErrorIdentifier.ARTIFACT_DOWNLOAD_ERROR));
        EasyMock.replay(mMockResolver);
        try {
            setter.validateRemoteFilePath(mResolver);
            fail("Should have thrown an exception");
        } catch (BuildRetrievalError expected) {
            // Only when we reach failure/test it fails
            assertTrue(expected.getMessage().contains("retrieval error"));
        }
        EasyMock.verify(mMockResolver);
    }

    @Test
    public void testResolve_remoteMap() throws Exception {
        RemoteFileOption object = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object);

        File fake = temporaryFolder.newFile();
        File fake2 = temporaryFolder.newFile();

        setter.setOptionValue("remote-map", "gs://fake/path", "value");
        setter.setOptionValue("remote-map", "fake/file", "gs://fake/path2");
        setter.setOptionValue("remote-map", "key", "val");
        assertEquals(3, object.remoteMap.size());

        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")),
                                EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path2")),
                                EasyMock.anyObject()))
                .andReturn(fake2);
        EasyMock.replay(mMockResolver);

        Set<File> downloadedFile = setter.validateRemoteFilePath(mResolver);
        assertEquals(2, downloadedFile.size());
        // The file has been replaced by the downloaded one.
        assertEquals(3, object.remoteMap.size());
        assertEquals(new File("value"), object.remoteMap.get(fake));
        assertEquals(fake2, object.remoteMap.get(new File("fake/file")));
        assertEquals(new File("val"), object.remoteMap.get(new File("key")));
        EasyMock.verify(mMockResolver);
    }

    @Test
    public void testResolve_remoteMultiMap() throws Exception {
        RemoteFileOption object = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object);

        File fake = temporaryFolder.newFile();
        File fake2 = temporaryFolder.newFile();
        File fake3 = temporaryFolder.newFile();

        setter.setOptionValue("remote-multi-map", "gs://fake/path", "value");
        setter.setOptionValue("remote-multi-map", "fake/file", "gs://fake/path2");
        setter.setOptionValue("remote-multi-map", "fake/file", "gs://fake/path3");
        setter.setOptionValue("remote-multi-map", "key", "val");
        assertEquals(3, object.remoteMultiMap.size());

        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")),
                                EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path2")),
                                EasyMock.anyObject()))
                .andReturn(fake2);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path3")),
                                EasyMock.anyObject()))
                .andReturn(fake3);
        EasyMock.replay(mMockResolver);

        Set<File> downloadedFile = setter.validateRemoteFilePath(mResolver);
        assertEquals(3, downloadedFile.size());
        // The file has been replaced by the downloaded one.
        assertEquals(3, object.remoteMultiMap.size());
        assertEquals(new File("value"), object.remoteMultiMap.get(fake).get(0));
        assertEquals(fake2, object.remoteMultiMap.get(new File("fake/file")).get(0));
        assertEquals(fake3, object.remoteMultiMap.get(new File("fake/file")).get(1));
        assertEquals(new File("val"), object.remoteMultiMap.get(new File("key")).get(0));
        EasyMock.verify(mMockResolver);
    }

    @Test
    public void testResolve_remoteMultiMap_concurrent() throws Exception {
        RemoteFileOption object = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object);

        File fake = temporaryFolder.newFile();
        File fake2 = temporaryFolder.newFile();
        File fake3 = temporaryFolder.newFile();

        setter.setOptionValue("remote-multi-map", "fake/file", "gs://fake/path");
        setter.setOptionValue("remote-multi-map", "fake/file", "gs://fake/path2");
        setter.setOptionValue("remote-multi-map", "fake/file", "gs://fake/path3");
        assertEquals(1, object.remoteMultiMap.size());

        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")), EasyMock.anyObject()))
                .andAnswer(
                        new IAnswer<File>() {
                            @Override
                            public File answer() throws Throwable {
                                RunUtil.getDefault().sleep(1000);
                                return fake;
                            }
                        });
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path2")), EasyMock.anyObject()))
                .andReturn(fake2);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path3")), EasyMock.anyObject()))
                .andReturn(fake3);
        EasyMock.replay(mMockResolver);

        List<Callable<Set<File>>> call = new ArrayList<>();
        List<ITestDevice> devices = new ArrayList<>();
        for (int i = 0; i < 5; i++) {
            OptionSetter setter2 = new OptionSetter(object);
            Callable<Set<File>> callableTask =
                    () -> {
                        return setter2.validateRemoteFilePath(mResolver);
                    };
            call.add(callableTask);
            devices.add(Mockito.mock(ITestDevice.class));
        }
        ParallelDeviceExecutor<Set<File>> executor = new ParallelDeviceExecutor<>(devices);
        List<Set<File>> downloadedFile = null;
        downloadedFile = executor.invokeAll(call, 1, TimeUnit.MINUTES);
        boolean oneMustBeNonEmpty = false;
        for (Set<File> set : downloadedFile) {
            if (set.size() == 3) {
                oneMustBeNonEmpty = true;
            }
        }
        assertTrue(oneMustBeNonEmpty);
        // The file has been replaced by the downloaded one.
        assertEquals(1, object.remoteMultiMap.size());
        assertEquals(3, object.remoteMultiMap.values().size());
        assertEquals(fake, object.remoteMultiMap.get(new File("fake/file")).get(0));
        assertEquals(fake2, object.remoteMultiMap.get(new File("fake/file")).get(1));
        assertEquals(fake3, object.remoteMultiMap.get(new File("fake/file")).get(2));
        EasyMock.verify(mMockResolver);
    }

    @Test
    public void testResolve_withNoGlobalNameSpace() throws Exception {
        RemoteFileOptionWithOptionClass object = new RemoteFileOptionWithOptionClass();
        OptionSetter setter = new OptionSetter(object);

        File fake = temporaryFolder.newFile();

        setter.setOptionValue("option-class-alias:remote-file", "gs://fake/path");
        assertEquals("gs:/fake/path", object.remoteFile.getPath());

        // File is downloaded the first time, then is ignored since it doesn't have the protocol
        // anymore
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")),
                                EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.replay(mMockResolver);

        Set<File> downloadedFile = setter.validateRemoteFilePath(mResolver);
        assertEquals(1, downloadedFile.size());
        File downloaded = downloadedFile.iterator().next();
        // The file has been replaced by the downloaded one.
        assertEquals(downloaded.getAbsolutePath(), object.remoteFile.getAbsolutePath());
        EasyMock.verify(mMockResolver);
    }

    @Test
    public void testResolvePartialDownloadZip() throws Exception {
        List<String> includeFilters = Arrays.asList("test1", "test2");
        List<String> excludeFilters = Arrays.asList("[.]config");

        Map<String, String> queryArgs = new HashMap<>();
        queryArgs.put("partial_download_dir", "/tmp");
        queryArgs.put("include_filters", "test1;test2");
        queryArgs.put("exclude_filters", "[.]config");
        mMockResolver.setPrimaryDevice(null);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")),
                                EasyMock.eq(queryArgs)))
                .andReturn(null);
        EasyMock.replay(mMockResolver);

        mResolver.resolvePartialDownloadZip(
                new File("/tmp"), "gs:/fake/path", includeFilters, excludeFilters);
        EasyMock.verify(mMockResolver);
    }

    /** Ignore any error if the download request is optional. */
    @Test
    public void testResolvePartialDownloadZip_optional() throws Exception {
        List<String> includeFilters = Arrays.asList("test1", "test2");
        List<String> excludeFilters = Arrays.asList("[.]config");

        Map<String, String> queryArgs = new HashMap<>();
        queryArgs.put("partial_download_dir", "/tmp");
        queryArgs.put("include_filters", "test1;test2");
        queryArgs.put("exclude_filters", "[.]config");
        queryArgs.put("optional", "true");
        mMockResolver.setPrimaryDevice(null);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path?optional=true")),
                                EasyMock.eq(queryArgs)))
                .andThrow(
                        new BuildRetrievalError(
                                "should not throw this exception.",
                                InfraErrorIdentifier.ARTIFACT_UNSUPPORTED_PATH));
        EasyMock.replay(mMockResolver);

        mResolver.resolvePartialDownloadZip(
                new File("/tmp"), "gs:/fake/path?optional=true", includeFilters, excludeFilters);
        EasyMock.verify(mMockResolver);
    }

    /**
     * Ensure that the same field on two different objects can be set with different remote values.
     */
    @Test
    public void testResolveTwoObjects() throws Exception {
        RemoteFileOption object1 = new RemoteFileOption();
        RemoteFileOption object2 = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object1, object2);

        File fake = temporaryFolder.newFile();
        setter.setOptionValue("alias-remote-file:1:remote-file", "gs://fake/path");
        assertEquals("gs:/fake/path", object1.remoteFile.getPath());

        File fake2 = temporaryFolder.newFile();
        setter.setOptionValue("alias-remote-file:2:remote-file", "gs://fake2/path2");
        assertEquals("gs:/fake2/path2", object2.remoteFile.getPath());

        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")),
                                EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake2/path2")),
                                EasyMock.anyObject()))
                .andReturn(fake2);
        EasyMock.replay(mMockResolver);

        Set<File> downloadedFile = setter.validateRemoteFilePath(mResolver);
        assertEquals(2, downloadedFile.size());
        assertTrue(downloadedFile.contains(object1.remoteFile));
        assertTrue(downloadedFile.contains(object2.remoteFile));

        assertFalse(object1.remoteFile.equals(object2.remoteFile));
        EasyMock.verify(mMockResolver);
    }

    /** Ensure that if the same value is set on two different objects we still resolve both. */
    @Test
    public void testResolveTwoObjects_sameValue() throws Exception {
        RemoteFileOption object1 = new RemoteFileOption();
        RemoteFileOption object2 = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object1, object2);

        File fake = temporaryFolder.newFile();
        setter.setOptionValue("alias-remote-file:1:remote-file", "gs://fake/path");
        assertEquals("gs:/fake/path", object1.remoteFile.getPath());

        File fake2 = temporaryFolder.newFile();
        setter.setOptionValue("alias-remote-file:2:remote-file", "gs://fake/path");
        assertEquals("gs:/fake/path", object2.remoteFile.getPath());

        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")),
                                EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")),
                                EasyMock.anyObject()))
                .andReturn(fake2);
        EasyMock.replay(mMockResolver);

        Set<File> downloadedFile = setter.validateRemoteFilePath(mResolver);
        assertEquals(2, downloadedFile.size());
        assertTrue(downloadedFile.contains(object1.remoteFile));
        assertTrue(downloadedFile.contains(object2.remoteFile));

        assertFalse(object1.remoteFile.equals(object2.remoteFile));
        EasyMock.verify(mMockResolver);
    }

    /** Ensure that we are able to load all the services included in Tradefed. */
    @Test
    public void testServiceLoader() {
        ServiceLoader<IRemoteFileResolver> serviceLoader =
                ServiceLoader.load(IRemoteFileResolver.class);
        assertNotNull(serviceLoader);
        List<IRemoteFileResolver> listResolver = new ArrayList<>();
        serviceLoader.forEach(listResolver::add);
        // We want to ensure we were successful in loading resolvers, we need to load at least one
        // since we should always have a few.
        assertThat(listResolver).isNotEmpty();
    }

    @Test
    public void canResolveLocalFileUris() throws Exception {
        File fake = temporaryFolder.newFile();
        RemoteFileOption object = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object);
        setter.setOptionValue("remote-file", fake.toURI().toString());
        DynamicRemoteFileResolver resolver = new DynamicRemoteFileResolver();

        Set<File> downloadedFile = setter.validateRemoteFilePath(resolver);

        assertThat(object.remoteFile).isEqualTo(fake);
    }

    @Test
    public void canResolveUsingCustomResolvers() throws Exception {
        RemoteFileOption object = new RemoteFileOption();
        OptionSetter setter = new OptionSetter(object);
        setter.setOptionValue("remote-file", NullFileResolver.PROTOCOL + "://a-file");
        ClassLoader classLoader = classLoaderWithProviders(NullFileResolver.class.getName());
        DynamicRemoteFileResolver resolver =
                new DynamicRemoteFileResolver(new ServiceFileResolverLoader(classLoader));

        Set<File> downloadedFile = setter.validateRemoteFilePath(resolver);

        assertThat(object.remoteFile).isEqualTo(NullFileResolver.NULL_FILE);
    }

    @Test
    public void resolverLoader_multipleCallsReturnTheSameResolver() throws Exception {
        ClassLoader classLoader = classLoaderWithProviders(NullFileResolver.class.getName());
        FileResolverLoader loader = new ServiceFileResolverLoader(classLoader);
        IRemoteFileResolver expected = loader.load(NullFileResolver.PROTOCOL, ImmutableMap.of());

        IRemoteFileResolver actual = loader.load(NullFileResolver.PROTOCOL, ImmutableMap.of());

        assertThat(actual).isSameAs(expected);
    }

    @Test
    public void resolverLoader_differentInstancesReturnDifferentResolvers() throws Exception {
        ClassLoader classLoader = classLoaderWithProviders(NullFileResolver.class.getName());
        FileResolverLoader loader1 = new ServiceFileResolverLoader(classLoader);
        FileResolverLoader loader2 = new ServiceFileResolverLoader(classLoader);

        IRemoteFileResolver resolver1 = loader1.load(NullFileResolver.PROTOCOL, ImmutableMap.of());
        IRemoteFileResolver resolver2 = loader2.load(NullFileResolver.PROTOCOL, ImmutableMap.of());

        assertThat(resolver1).isNotSameAs(resolver2);
    }

    @Test
    public void resolverLoader_canHandleMultipleResolversWithSameScheme() throws Exception {
        ClassLoader classLoader =
                classLoaderWithProviders(
                        NullFileResolver.class.getName(),
                        DuplicateNullFileResolver.class.getName());
        FileResolverLoader loader = new ServiceFileResolverLoader(classLoader);

        IRemoteFileResolver resolver = loader.load(NullFileResolver.PROTOCOL, ImmutableMap.of());

        assertThat(resolver.getClass())
                .isAnyOf(NullFileResolver.class, DuplicateNullFileResolver.class);
    }

    @Test
    public void resolverLoader_throwsIfMissingMandatoryOption() throws Exception {
        ClassLoader classLoader = classLoaderWithProviders(ResolverWithOptions.class.getName());
        FileResolverLoader loader = new ServiceFileResolverLoader(classLoader);
        Map<String, String> config = ResolverWithOptions.minimalConfig();
        config.remove(ResolverWithOptions.MANDATORY_OPTION_FQN);

        try {
            loader.load(ResolverWithOptions.PROTOCOL, config);
            fail();
        } catch (ResolverLoadingException expected) {
            assertThat(expected)
                    .hasCauseThat()
                    .hasMessageThat()
                    .contains(ResolverWithOptions.MANDATORY_OPTION_NAME);
        }
    }

    @Test
    public void resolverLoader_setsNonMandatoryOption() throws Exception {
        String optionValue = "value";
        ClassLoader classLoader = classLoaderWithProviders(ResolverWithOptions.class.getName());
        FileResolverLoader loader = new ServiceFileResolverLoader(classLoader);
        Map<String, String> config = ResolverWithOptions.minimalConfig();
        config.put(ResolverWithOptions.NON_MANDATORY_OPTION_FQN, optionValue);

        IRemoteFileResolver resolver = loader.load(ResolverWithOptions.PROTOCOL, config);

        assertThat(((ResolverWithOptions) resolver).nonMandatoryOption).isEqualTo(optionValue);
    }

    @Test
    public void resolverLoader_throwsForMapOption() throws Exception {
        ClassLoader classLoader = classLoaderWithProviders(ResolverWithOptions.class.getName());
        FileResolverLoader loader = new ServiceFileResolverLoader(classLoader);
        Map<String, String> config = ResolverWithOptions.minimalConfig();
        config.put(ResolverWithOptions.MAP_OPTION_FQN, "key=value");

        try {
            loader.load(ResolverWithOptions.PROTOCOL, config);
            fail();
        } catch (ResolverLoadingException expected) {
            assertThat(expected)
                    .hasCauseThat()
                    .hasMessageThat()
                    .contains(ResolverWithOptions.MAP_OPTION_FQN);
            assertThat(expected).hasCauseThat().hasMessageThat().contains("not supported");
        }
    }

    @Test
    public void resolverLoader_resolvesFileOption() throws Exception {
        File file = temporaryFolder.newFile();
        ClassLoader classLoader = classLoaderWithProviders(ResolverWithOptions.class.getName());
        FileResolverLoader loader = new ServiceFileResolverLoader(classLoader);
        Map<String, String> config = ResolverWithOptions.minimalConfig();
        config.put(ResolverWithOptions.MANDATORY_OPTION_FQN, file.toURI().toString());

        IRemoteFileResolver resolver = loader.load(ResolverWithOptions.PROTOCOL, config);

        assertThat(((ResolverWithOptions) resolver).mandatoryOption).isEqualTo(file);
    }

    @Test
    public void resolverLoader_doesNotResolveFileOptionWithUnsupportedScheme() throws Exception {
        ClassLoader classLoader = classLoaderWithProviders(ResolverWithOptions.class.getName());
        FileResolverLoader loader = new ServiceFileResolverLoader(classLoader);
        Map<String, String> config = ResolverWithOptions.minimalConfig();
        config.put(ResolverWithOptions.MANDATORY_OPTION_FQN, "missing://tmp");

        IRemoteFileResolver resolver = loader.load(ResolverWithOptions.PROTOCOL, config);

        assertThat(((ResolverWithOptions) resolver).mandatoryOption.toString())
                .contains("missing:");
    }

    @Test
    public void resolverLoader_resolvesResolverFileOption() throws Exception {
        File f = new File("/tmp/a-file");
        ClassLoader classLoader =
                classLoaderWithProviders(
                        ResolverWithOptions.class
                                .getName(), // Contains a file option resolved by the next.
                        AnotherResolverWithOptions.class.getName());
        FileResolverLoader loader = new ServiceFileResolverLoader(classLoader);
        Map<String, String> config = new HashMap<>();
        config.putAll(ResolverWithOptions.minimalConfig());
        config.putAll(AnotherResolverWithOptions.minimalConfig());
        config.put(
                ResolverWithOptions.MANDATORY_OPTION_FQN,
                AnotherResolverWithOptions.PROTOCOL + "://a-file");
        config.put(AnotherResolverWithOptions.MANDATORY_OPTION_FQN, f.toString());

        IRemoteFileResolver resolver = loader.load(ResolverWithOptions.PROTOCOL, config);

        assertThat(((ResolverWithOptions) resolver).mandatoryOption).isEqualTo(f);
    }

    @Test
    public void resolverLoader_throwsOnInitializationCycles() throws Exception {
        ClassLoader classLoader = classLoaderWithProviders(ResolverWithOptions.class.getName());
        FileResolverLoader loader = new ServiceFileResolverLoader(classLoader);
        Map<String, String> config = ResolverWithOptions.minimalConfig();
        config.put(
                ResolverWithOptions.MANDATORY_OPTION_FQN,
                ResolverWithOptions.PROTOCOL + "://a-file");

        try {
            loader.load(ResolverWithOptions.PROTOCOL, config);
            fail();
        } catch (ResolverLoadingException expected) {
            assertThat(expected)
                    .hasCauseThat()
                    .hasCauseThat()
                    .hasMessageThat()
                    .contains("Cycle detected");
        }
    }

    @Test
    public void resolverLoader_onlyInitializesOptionsOfUsedResolvers() throws Exception {
        File f = new File("/tmp/a-file");
        ClassLoader classLoader =
                classLoaderWithProviders(
                        ResolverWithOptions.class.getName(),
                        AnotherResolverWithOptions.class
                                .getName()); // Requires option but never used.
        FileResolverLoader loader = new ServiceFileResolverLoader(classLoader);
        Map<String, String> config = new HashMap<>();
        config.putAll(ResolverWithOptions.minimalConfig());
        config.put(ResolverWithOptions.MANDATORY_OPTION_FQN, f.toString());

        IRemoteFileResolver resolver = loader.load(ResolverWithOptions.PROTOCOL, config);

        assertThat(((ResolverWithOptions) resolver).mandatoryOption).isEqualTo(f);
    }

    public static final class ResolverWithOptions implements IRemoteFileResolver {
        static final String PROTOCOL = "rwo";
        static final String MANDATORY_OPTION_NAME = "mandatory";
        static final String MANDATORY_OPTION_FQN =
                optionFqn(ResolverWithOptions.class, MANDATORY_OPTION_NAME);
        static final String NON_MANDATORY_OPTION_NAME = "nonMandatory";
        static final String NON_MANDATORY_OPTION_FQN =
                optionFqn(ResolverWithOptions.class, NON_MANDATORY_OPTION_NAME);
        static final String MAP_OPTION_NAME = "map";
        static final String MAP_OPTION_FQN = optionFqn(ResolverWithOptions.class, MAP_OPTION_NAME);

        static Map<String, String> minimalConfig() {
            Map<String, String> config = new HashMap<>();
            config.put(MANDATORY_OPTION_FQN, "anything");
            return config;
        }

        @Option(name = MANDATORY_OPTION_NAME, mandatory = true)
        private File mandatoryOption;

        @Option(name = NON_MANDATORY_OPTION_NAME)
        private String nonMandatoryOption;

        @Option(name = MAP_OPTION_NAME)
        private Map<String, String> mapOption;

        @Override
        public String getSupportedProtocol() {
            return PROTOCOL;
        }

        @Override
        public File resolveRemoteFiles(File consideredFile, Map<String, String> queryArgs) {
            return mandatoryOption;
        }
    }

    public static final class AnotherResolverWithOptions implements IRemoteFileResolver {
        static final String PROTOCOL = "arwo";
        static final String MANDATORY_OPTION_NAME = "mandatory";
        static final String MANDATORY_OPTION_FQN =
                optionFqn(AnotherResolverWithOptions.class, MANDATORY_OPTION_NAME);

        static Map<String, String> minimalConfig() {
            Map<String, String> config = new HashMap<>();
            config.put(MANDATORY_OPTION_FQN, "anything");
            return config;
        }

        @Option(name = MANDATORY_OPTION_NAME, mandatory = true)
        private File mandatoryOption;

        @Override
        public String getSupportedProtocol() {
            return PROTOCOL;
        }

        @Override
        public File resolveRemoteFiles(File consideredFile, Map<String, String> queryArgs) {
            return mandatoryOption;
        }
    }

    private static String optionFqn(Class<?> cls, String name) {
        return cls.getName() + ":" + name;
    }

    @Test
    public void testMultiDevices() throws Exception {
        IConfiguration configuration = new Configuration("test", "test");

        List<IDeviceConfiguration> listConfigs = new ArrayList<>();
        IDeviceConfiguration holder1 = new DeviceConfigurationHolder("device1");
        BootstrapBuildProvider provider1 = new BootstrapBuildProvider();
        OptionSetter setter = new OptionSetter(provider1);
        setter.setOptionValue("tests-dir", "gs://fake/path");
        holder1.addSpecificConfig(provider1);
        listConfigs.add(holder1);

        IDeviceConfiguration holder2 = new DeviceConfigurationHolder("device2");
        BootstrapBuildProvider provider2 = new BootstrapBuildProvider();
        OptionSetter setter2 = new OptionSetter(provider2);
        setter2.setOptionValue("tests-dir", "gs://fake/path");
        holder2.addSpecificConfig(provider2);
        listConfigs.add(holder2);

        configuration.setDeviceConfigList(listConfigs);

        File fake = temporaryFolder.newFile();
        File fake2 = temporaryFolder.newFile();

        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")), EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")), EasyMock.anyObject()))
                .andReturn(fake2);

        EasyMock.replay(mMockResolver);
        configuration.resolveDynamicOptions(mResolver);
        try {
            assertEquals(fake, provider1.getTestsDir());
            assertEquals(fake2, provider2.getTestsDir());
        } finally {
            configuration.cleanConfigurationData();
        }
        EasyMock.verify(mMockResolver);
    }

    @Test
    public void testMultiDevices_replicat() throws Exception {
        IConfiguration configuration =
                new Configuration("test", "test") {
                    @Override
                    protected boolean isRemoteEnvironment() {
                        return true;
                    }
                };
        configuration.getCommandOptions().setReplicateSetup(true);
        configuration.getCommandOptions().setShardCount(2);
        configuration.setCommandLine(
                new String[] {"tf/bootstrap", "--tests-dir", "gs://fake/path"});

        List<IDeviceConfiguration> listConfigs = new ArrayList<>();
        IDeviceConfiguration holder1 =
                new DeviceConfigurationHolder(ConfigurationDef.DEFAULT_DEVICE_NAME);
        BootstrapBuildProvider provider1 = new BootstrapBuildProvider();
        OptionSetter setter = new OptionSetter(provider1);
        setter.setOptionValue("tests-dir", "gs://fake/path");
        holder1.addSpecificConfig(provider1);
        listConfigs.add(holder1);

        configuration.setDeviceConfigList(listConfigs);

        assertEquals(1, configuration.getDeviceConfig().size());
        ParentShardReplicate.replicatedSetup(configuration, null);
        assertEquals(2, configuration.getDeviceConfig().size());

        assertTrue(
                configuration.getDeviceConfig().get(1).getBuildProvider()
                        instanceof StubBuildProvider);

        File fake = temporaryFolder.newFile();
        EasyMock.expect(
                        mMockResolver.resolveRemoteFiles(
                                EasyMock.eq(new File("gs:/fake/path")), EasyMock.anyObject()))
                .andReturn(fake);
        EasyMock.replay(mMockResolver);
        configuration.resolveDynamicOptions(mResolver);
        try {
            assertEquals(fake, provider1.getTestsDir());
        } finally {
            configuration.cleanConfigurationData();
        }
        EasyMock.verify(mMockResolver);
    }

    private ClassLoader classLoaderWithProviders(String... lines) throws IOException {
        String service = IRemoteFileResolver.class.getName();
        File jar = temporaryFolder.newFile();

        try (JarOutputStream out = new JarOutputStream(new FileOutputStream(jar))) {
            JarEntry jarEntry = new JarEntry("META-INF/services/" + service);

            out.putNextEntry(jarEntry);
            PrintWriter writer =
                    new PrintWriter(new OutputStreamWriter(out, StandardCharsets.UTF_8));

            for (String line : lines) {
                writer.println(line);
            }

            writer.flush();
        }

        return new URLClassLoader(new URL[] {jar.toURI().toURL()});
    }

    public static class NullFileResolver implements IRemoteFileResolver {
        private static final String PROTOCOL = "null";
        private static final File NULL_FILE = new File("/dev/null");

        @Override
        public String getSupportedProtocol() {
            return PROTOCOL;
        }

        @Override
        public File resolveRemoteFiles(File consideredFile, Map<String, String> queryArgs) {
            return NULL_FILE;
        }
    }

    public static final class DuplicateNullFileResolver extends NullFileResolver {}

    private static final Correspondence<File, File> FILE_PATH_EQUIVALENCE =
            new Correspondence<File, File>() {
                @Override
                public boolean compare(File actual, File expected) {
                    return expected.getAbsolutePath().equals(actual.getAbsolutePath());
                }

                @Override
                public String toString() {
                    return "is equivalent to";
                }
            };
}
