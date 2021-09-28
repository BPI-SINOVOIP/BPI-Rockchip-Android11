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

import android.os.LocaleList;
import android.os.ParcelFileDescriptor;
import android.text.TextUtils;
import androidx.annotation.GuardedBy;
import com.android.textclassifier.common.base.TcLog;
import com.android.textclassifier.common.logging.ResultIdUtils.ModelInfo;
import com.google.common.base.Optional;
import com.google.common.base.Preconditions;
import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableList;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.Objects;
import java.util.function.Function;
import java.util.function.Supplier;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Collectors;
import javax.annotation.Nullable;

/** Manages model files that are listed by the model files supplier. */
final class ModelFileManager {
  private static final String TAG = "ModelFileManager";

  private final Supplier<ImmutableList<ModelFile>> modelFileSupplier;

  public ModelFileManager(Supplier<ImmutableList<ModelFile>> modelFileSupplier) {
    this.modelFileSupplier = Preconditions.checkNotNull(modelFileSupplier);
  }

  /** Returns an immutable list of model files listed by the given model files supplier. */
  public ImmutableList<ModelFile> listModelFiles() {
    return modelFileSupplier.get();
  }

  /**
   * Returns the best model file for the given localelist, {@code null} if nothing is found.
   *
   * @param localeList the required locales, use {@code null} if there is no preference.
   */
  public ModelFile findBestModelFile(@Nullable LocaleList localeList) {
    final String languages =
        localeList == null || localeList.isEmpty()
            ? LocaleList.getDefault().toLanguageTags()
            : localeList.toLanguageTags();
    final List<Locale.LanguageRange> languageRangeList = Locale.LanguageRange.parse(languages);

    ModelFile bestModel = null;
    for (ModelFile model : listModelFiles()) {
      if (model.isAnyLanguageSupported(languageRangeList)) {
        if (model.isPreferredTo(bestModel)) {
          bestModel = model;
        }
      }
    }
    return bestModel;
  }

  /** Default implementation of the model file supplier. */
  public static final class ModelFileSupplierImpl implements Supplier<ImmutableList<ModelFile>> {
    private final File updatedModelFile;
    private final File factoryModelDir;
    private final Pattern modelFilenamePattern;
    private final Function<Integer, Integer> versionSupplier;
    private final Function<Integer, String> supportedLocalesSupplier;
    private final Object lock = new Object();

    @GuardedBy("lock")
    private ImmutableList<ModelFile> factoryModels;

    public ModelFileSupplierImpl(
        File factoryModelDir,
        String factoryModelFileNameRegex,
        File updatedModelFile,
        Function<Integer, Integer> versionSupplier,
        Function<Integer, String> supportedLocalesSupplier) {
      this.updatedModelFile = Preconditions.checkNotNull(updatedModelFile);
      this.factoryModelDir = Preconditions.checkNotNull(factoryModelDir);
      modelFilenamePattern = Pattern.compile(Preconditions.checkNotNull(factoryModelFileNameRegex));
      this.versionSupplier = Preconditions.checkNotNull(versionSupplier);
      this.supportedLocalesSupplier = Preconditions.checkNotNull(supportedLocalesSupplier);
    }

    @Override
    public ImmutableList<ModelFile> get() {
      final List<ModelFile> modelFiles = new ArrayList<>();
      // The update model has the highest precedence.
      if (updatedModelFile.exists()) {
        final ModelFile updatedModel = createModelFile(updatedModelFile);
        if (updatedModel != null) {
          modelFiles.add(updatedModel);
        }
      }
      // Factory models should never have overlapping locales, so the order doesn't matter.
      synchronized (lock) {
        if (factoryModels == null) {
          factoryModels = getFactoryModels();
        }
        modelFiles.addAll(factoryModels);
      }
      return ImmutableList.copyOf(modelFiles);
    }

    private ImmutableList<ModelFile> getFactoryModels() {
      List<ModelFile> factoryModelFiles = new ArrayList<>();
      if (factoryModelDir.exists() && factoryModelDir.isDirectory()) {
        final File[] files = factoryModelDir.listFiles();
        for (File file : files) {
          final Matcher matcher = modelFilenamePattern.matcher(file.getName());
          if (matcher.matches() && file.isFile()) {
            final ModelFile model = createModelFile(file);
            if (model != null) {
              factoryModelFiles.add(model);
            }
          }
        }
      }
      return ImmutableList.copyOf(factoryModelFiles);
    }

    /** Returns null if the path did not point to a compatible model. */
    @Nullable
    private ModelFile createModelFile(File file) {
      if (!file.exists()) {
        return null;
      }
      ParcelFileDescriptor modelFd = null;
      try {
        modelFd = ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
        if (modelFd == null) {
          return null;
        }
        final int modelFdInt = modelFd.getFd();
        final int version = versionSupplier.apply(modelFdInt);
        final String supportedLocalesStr = supportedLocalesSupplier.apply(modelFdInt);
        if (supportedLocalesStr.isEmpty()) {
          TcLog.d(TAG, "Ignoring " + file.getAbsolutePath());
          return null;
        }
        final List<Locale> supportedLocales = new ArrayList<>();
        for (String langTag : Splitter.on(',').split(supportedLocalesStr)) {
          supportedLocales.add(Locale.forLanguageTag(langTag));
        }
        return new ModelFile(
            file,
            version,
            supportedLocales,
            supportedLocalesStr,
            ModelFile.LANGUAGE_INDEPENDENT.equals(supportedLocalesStr));
      } catch (FileNotFoundException e) {
        TcLog.e(TAG, "Failed to find " + file.getAbsolutePath(), e);
        return null;
      } finally {
        maybeCloseAndLogError(modelFd);
      }
    }

    /** Closes the ParcelFileDescriptor, if non-null, and logs any errors that occur. */
    private static void maybeCloseAndLogError(@Nullable ParcelFileDescriptor fd) {
      if (fd == null) {
        return;
      }
      try {
        fd.close();
      } catch (IOException e) {
        TcLog.e(TAG, "Error closing file.", e);
      }
    }
  }

  /** Describes TextClassifier model files on disk. */
  public static final class ModelFile {
    public static final String LANGUAGE_INDEPENDENT = "*";

    private final File file;
    private final int version;
    private final List<Locale> supportedLocales;
    private final String supportedLocalesStr;
    private final boolean languageIndependent;

    public ModelFile(
        File file,
        int version,
        List<Locale> supportedLocales,
        String supportedLocalesStr,
        boolean languageIndependent) {
      this.file = Preconditions.checkNotNull(file);
      this.version = version;
      this.supportedLocales = Preconditions.checkNotNull(supportedLocales);
      this.supportedLocalesStr = Preconditions.checkNotNull(supportedLocalesStr);
      this.languageIndependent = languageIndependent;
    }

    /** Returns the absolute path to the model file. */
    public String getPath() {
      return file.getAbsolutePath();
    }

    /** Returns a name to use for id generation, effectively the name of the model file. */
    public String getName() {
      return file.getName();
    }

    /** Returns the version tag in the model's metadata. */
    public int getVersion() {
      return version;
    }

    /** Returns whether the language supports any language in the given ranges. */
    public boolean isAnyLanguageSupported(List<Locale.LanguageRange> languageRanges) {
      Preconditions.checkNotNull(languageRanges);
      return languageIndependent || Locale.lookup(languageRanges, supportedLocales) != null;
    }

    /** Returns an immutable lists of supported locales. */
    public List<Locale> getSupportedLocales() {
      return Collections.unmodifiableList(supportedLocales);
    }

    /** Returns the original supported locals string read from the model file. */
    public String getSupportedLocalesStr() {
      return supportedLocalesStr;
    }

    /** Returns if this model file is preferred to the given one. */
    public boolean isPreferredTo(@Nullable ModelFile model) {
      // A model is preferred to no model.
      if (model == null) {
        return true;
      }

      // A language-specific model is preferred to a language independent
      // model.
      if (!languageIndependent && model.languageIndependent) {
        return true;
      }
      if (languageIndependent && !model.languageIndependent) {
        return false;
      }

      // A higher-version model is preferred.
      if (version > model.getVersion()) {
        return true;
      }
      return false;
    }

    @Override
    public int hashCode() {
      return Objects.hash(getPath());
    }

    @Override
    public boolean equals(Object other) {
      if (this == other) {
        return true;
      }
      if (other instanceof ModelFile) {
        final ModelFile otherModel = (ModelFile) other;
        return TextUtils.equals(getPath(), otherModel.getPath());
      }
      return false;
    }

    public ModelInfo toModelInfo() {
      return new ModelInfo(getVersion(), supportedLocalesStr);
    }

    @Override
    public String toString() {
      return String.format(
          Locale.US,
          "ModelFile { path=%s name=%s version=%d locales=%s }",
          getPath(),
          getName(),
          version,
          supportedLocalesStr);
    }

    public static ImmutableList<Optional<ModelInfo>> toModelInfos(
        Optional<ModelFile>... modelFiles) {
      return Arrays.stream(modelFiles)
          .map(modelFile -> modelFile.transform(ModelFile::toModelInfo))
          .collect(Collectors.collectingAndThen(Collectors.toList(), ImmutableList::copyOf));
    }
  }
}
