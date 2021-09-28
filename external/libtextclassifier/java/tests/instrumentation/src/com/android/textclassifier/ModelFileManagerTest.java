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

package com.android.textclassifier;

import static com.google.common.truth.Truth.assertThat;
import static org.mockito.Mockito.when;

import android.os.LocaleList;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.SmallTest;
import com.android.textclassifier.ModelFileManager.ModelFile;
import com.android.textclassifier.common.logging.ResultIdUtils.ModelInfo;
import com.google.common.base.Optional;
import com.google.common.collect.ImmutableList;
import java.io.File;
import java.io.IOException;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.function.Supplier;
import java.util.stream.Collectors;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class ModelFileManagerTest {
  private static final Locale DEFAULT_LOCALE = Locale.forLanguageTag("en-US");
  @Mock private Supplier<ImmutableList<ModelFile>> modelFileSupplier;
  private ModelFileManager.ModelFileSupplierImpl modelFileSupplierImpl;
  private ModelFileManager modelFileManager;
  private File rootTestDir;
  private File factoryModelDir;
  private File updatedModelFile;

  @Before
  public void setup() {
    MockitoAnnotations.initMocks(this);
    modelFileManager = new ModelFileManager(modelFileSupplier);
    rootTestDir = ApplicationProvider.getApplicationContext().getCacheDir();
    factoryModelDir = new File(rootTestDir, "factory");
    updatedModelFile = new File(rootTestDir, "updated.model");

    modelFileSupplierImpl =
        new ModelFileManager.ModelFileSupplierImpl(
            factoryModelDir,
            "test\\d.model",
            updatedModelFile,
            fd -> 1,
            fd -> ModelFileManager.ModelFile.LANGUAGE_INDEPENDENT);

    rootTestDir.mkdirs();
    factoryModelDir.mkdirs();

    Locale.setDefault(DEFAULT_LOCALE);
  }

  @After
  public void removeTestDir() {
    recursiveDelete(rootTestDir);
  }

  @Test
  public void get() {
    ModelFileManager.ModelFile modelFile =
        new ModelFileManager.ModelFile(new File("/path/a"), 1, ImmutableList.of(), "", true);
    when(modelFileSupplier.get()).thenReturn(ImmutableList.of(modelFile));

    List<ModelFileManager.ModelFile> modelFiles = modelFileManager.listModelFiles();

    assertThat(modelFiles).hasSize(1);
    assertThat(modelFiles.get(0)).isEqualTo(modelFile);
  }

  @Test
  public void findBestModel_versionCode() {
    ModelFileManager.ModelFile olderModelFile =
        new ModelFileManager.ModelFile(new File("/path/a"), 1, ImmutableList.of(), "", true);

    ModelFileManager.ModelFile newerModelFile =
        new ModelFileManager.ModelFile(new File("/path/b"), 2, ImmutableList.of(), "", true);
    when(modelFileSupplier.get()).thenReturn(ImmutableList.of(olderModelFile, newerModelFile));

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(LocaleList.getEmptyLocaleList());

    assertThat(bestModelFile).isEqualTo(newerModelFile);
  }

  @Test
  public void findBestModel_languageDependentModelIsPreferred() {
    Locale locale = Locale.forLanguageTag("ja");
    ModelFileManager.ModelFile languageIndependentModelFile =
        new ModelFileManager.ModelFile(new File("/path/a"), 1, ImmutableList.of(), "", true);

    ModelFileManager.ModelFile languageDependentModelFile =
        new ModelFileManager.ModelFile(
            new File("/path/b"),
            1,
            Collections.singletonList(locale),
            locale.toLanguageTag(),
            false);
    when(modelFileSupplier.get())
        .thenReturn(ImmutableList.of(languageIndependentModelFile, languageDependentModelFile));

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(LocaleList.forLanguageTags(locale.toLanguageTag()));
    assertThat(bestModelFile).isEqualTo(languageDependentModelFile);
  }

  @Test
  public void findBestModel_noMatchedLanguageModel() {
    Locale locale = Locale.forLanguageTag("ja");
    ModelFileManager.ModelFile languageIndependentModelFile =
        new ModelFileManager.ModelFile(new File("/path/a"), 1, Collections.emptyList(), "", true);

    ModelFileManager.ModelFile languageDependentModelFile =
        new ModelFileManager.ModelFile(
            new File("/path/b"),
            1,
            Collections.singletonList(locale),
            locale.toLanguageTag(),
            false);

    when(modelFileSupplier.get())
        .thenReturn(ImmutableList.of(languageIndependentModelFile, languageDependentModelFile));

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(LocaleList.forLanguageTags("zh-hk"));
    assertThat(bestModelFile).isEqualTo(languageIndependentModelFile);
  }

  @Test
  public void findBestModel_noMatchedLanguageModel_defaultLocaleModelExists() {
    ModelFileManager.ModelFile languageIndependentModelFile =
        new ModelFileManager.ModelFile(new File("/path/a"), 1, ImmutableList.of(), "", true);

    ModelFileManager.ModelFile languageDependentModelFile =
        new ModelFileManager.ModelFile(
            new File("/path/b"),
            1,
            Collections.singletonList(DEFAULT_LOCALE),
            DEFAULT_LOCALE.toLanguageTag(),
            false);

    when(modelFileSupplier.get())
        .thenReturn(ImmutableList.of(languageIndependentModelFile, languageDependentModelFile));

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(LocaleList.forLanguageTags("zh-hk"));
    assertThat(bestModelFile).isEqualTo(languageIndependentModelFile);
  }

  @Test
  public void findBestModel_languageIsMoreImportantThanVersion() {
    ModelFileManager.ModelFile matchButOlderModel =
        new ModelFileManager.ModelFile(
            new File("/path/a"),
            1,
            Collections.singletonList(Locale.forLanguageTag("fr")),
            "fr",
            false);

    ModelFileManager.ModelFile mismatchButNewerModel =
        new ModelFileManager.ModelFile(
            new File("/path/b"),
            2,
            Collections.singletonList(Locale.forLanguageTag("ja")),
            "ja",
            false);

    when(modelFileSupplier.get())
        .thenReturn(ImmutableList.of(matchButOlderModel, mismatchButNewerModel));

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(LocaleList.forLanguageTags("fr"));
    assertThat(bestModelFile).isEqualTo(matchButOlderModel);
  }

  @Test
  public void findBestModel_languageIsMoreImportantThanVersion_bestModelComesFirst() {
    ModelFileManager.ModelFile matchLocaleModel =
        new ModelFileManager.ModelFile(
            new File("/path/b"),
            1,
            Collections.singletonList(Locale.forLanguageTag("ja")),
            "ja",
            false);

    ModelFileManager.ModelFile languageIndependentModel =
        new ModelFileManager.ModelFile(new File("/path/a"), 2, ImmutableList.of(), "", true);
    when(modelFileSupplier.get())
        .thenReturn(ImmutableList.of(matchLocaleModel, languageIndependentModel));

    ModelFileManager.ModelFile bestModelFile =
        modelFileManager.findBestModelFile(LocaleList.forLanguageTags("ja"));

    assertThat(bestModelFile).isEqualTo(matchLocaleModel);
  }

  @Test
  public void modelFileEquals() {
    ModelFileManager.ModelFile modelA =
        new ModelFileManager.ModelFile(
            new File("/path/a"),
            1,
            Collections.singletonList(Locale.forLanguageTag("ja")),
            "ja",
            false);

    ModelFileManager.ModelFile modelB =
        new ModelFileManager.ModelFile(
            new File("/path/a"),
            1,
            Collections.singletonList(Locale.forLanguageTag("ja")),
            "ja",
            false);

    assertThat(modelA).isEqualTo(modelB);
  }

  @Test
  public void modelFile_different() {
    ModelFileManager.ModelFile modelA =
        new ModelFileManager.ModelFile(
            new File("/path/a"),
            1,
            Collections.singletonList(Locale.forLanguageTag("ja")),
            "ja",
            false);

    ModelFileManager.ModelFile modelB =
        new ModelFileManager.ModelFile(
            new File("/path/b"),
            1,
            Collections.singletonList(Locale.forLanguageTag("ja")),
            "ja",
            false);

    assertThat(modelA).isNotEqualTo(modelB);
  }

  @Test
  public void modelFile_getPath() {
    ModelFileManager.ModelFile modelA =
        new ModelFileManager.ModelFile(
            new File("/path/a"),
            1,
            Collections.singletonList(Locale.forLanguageTag("ja")),
            "ja",
            false);

    assertThat(modelA.getPath()).isEqualTo("/path/a");
  }

  @Test
  public void modelFile_getName() {
    ModelFileManager.ModelFile modelA =
        new ModelFileManager.ModelFile(
            new File("/path/a"),
            1,
            Collections.singletonList(Locale.forLanguageTag("ja")),
            "ja",
            false);

    assertThat(modelA.getName()).isEqualTo("a");
  }

  @Test
  public void modelFile_isPreferredTo_languageDependentIsBetter() {
    ModelFileManager.ModelFile modelA =
        new ModelFileManager.ModelFile(
            new File("/path/a"),
            1,
            Collections.singletonList(Locale.forLanguageTag("ja")),
            "ja",
            false);

    ModelFileManager.ModelFile modelB =
        new ModelFileManager.ModelFile(new File("/path/b"), 2, ImmutableList.of(), "", true);

    assertThat(modelA.isPreferredTo(modelB)).isTrue();
  }

  @Test
  public void modelFile_isPreferredTo_version() {
    ModelFileManager.ModelFile modelA =
        new ModelFileManager.ModelFile(
            new File("/path/a"),
            2,
            Collections.singletonList(Locale.forLanguageTag("ja")),
            "ja",
            false);

    ModelFileManager.ModelFile modelB =
        new ModelFileManager.ModelFile(new File("/path/b"), 1, Collections.emptyList(), "", false);

    assertThat(modelA.isPreferredTo(modelB)).isTrue();
  }

  @Test
  public void modelFile_toModelInfo() {
    ModelFileManager.ModelFile modelFile =
        new ModelFileManager.ModelFile(
            new File("/path/a"), 2, ImmutableList.of(Locale.JAPANESE), "ja", false);

    ModelInfo modelInfo = modelFile.toModelInfo();

    assertThat(modelInfo.toModelName()).isEqualTo("ja_v2");
  }

  @Test
  public void modelFile_toModelInfos() {
    ModelFile englishModelFile =
        new ModelFile(new File("/path/a"), 1, ImmutableList.of(Locale.ENGLISH), "en", false);
    ModelFile japaneseModelFile =
        new ModelFile(new File("/path/a"), 2, ImmutableList.of(Locale.JAPANESE), "ja", false);

    ImmutableList<Optional<ModelInfo>> modelInfos =
        ModelFileManager.ModelFile.toModelInfos(
            Optional.of(englishModelFile), Optional.of(japaneseModelFile));

    assertThat(
            modelInfos.stream()
                .map(modelFile -> modelFile.transform(ModelInfo::toModelName).or(""))
                .collect(Collectors.toList()))
        .containsExactly("en_v1", "ja_v2")
        .inOrder();
  }

  @Test
  public void testFileSupplierImpl_updatedFileOnly() throws IOException {
    updatedModelFile.createNewFile();
    File model1 = new File(factoryModelDir, "test1.model");
    model1.createNewFile();
    File model2 = new File(factoryModelDir, "test2.model");
    model2.createNewFile();
    new File(factoryModelDir, "not_match_regex.model").createNewFile();

    List<ModelFileManager.ModelFile> modelFiles = modelFileSupplierImpl.get();
    List<String> modelFilePaths =
        modelFiles.stream().map(modelFile -> modelFile.getPath()).collect(Collectors.toList());

    assertThat(modelFiles).hasSize(3);
    assertThat(modelFilePaths)
        .containsExactly(
            updatedModelFile.getAbsolutePath(), model1.getAbsolutePath(), model2.getAbsolutePath());
  }

  @Test
  public void testFileSupplierImpl_empty() {
    factoryModelDir.delete();
    List<ModelFileManager.ModelFile> modelFiles = modelFileSupplierImpl.get();

    assertThat(modelFiles).hasSize(0);
  }

  private static void recursiveDelete(File f) {
    if (f.isDirectory()) {
      for (File innerFile : f.listFiles()) {
        recursiveDelete(innerFile);
      }
    }
    f.delete();
  }
}
