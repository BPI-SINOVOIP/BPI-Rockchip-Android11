/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.google.doclava;

import com.google.clearsilver.jsilver.data.Data;

import org.xml.sax.Attributes;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;
import org.xml.sax.XMLReader;
import org.xml.sax.helpers.DefaultHandler;
import org.xml.sax.helpers.XMLReaderFactory;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

public class CompatInfo {

  public static class CompatChange {
    public final String name;
    public final long id;
    public final String description;
    public final String definedInClass;
    public final String sourceFile;
    public final int sourceLine;
    public final boolean disabled;
    public final boolean loggingOnly;
    public final int enableAfterTargetSdk;


    CompatChange(String name, long id, String description, String definedInClass,
            String sourceFile, int sourceLine, boolean disabled, boolean loggingOnly,
            int enableAfterTargetSdk) {
      this.name = name;
      this.id = id;
      this.description = description;
      this.definedInClass = definedInClass;
      this.sourceFile = sourceFile;
      this.sourceLine = sourceLine;
      this.disabled = disabled;
      this.loggingOnly = loggingOnly;
      this.enableAfterTargetSdk = enableAfterTargetSdk;
    }

    static class Builder {
      private String mName;
      private long mId;
      private String mDescription;
      private String mDefinedInClass;
      private String mSourceFile;
      private int mSourceLine;
      private boolean mDisabled;
      private boolean mLoggingOnly;
      private int mEnableAfterTargetSdk;

      CompatChange build() {
        return new CompatChange(
            mName, mId, mDescription, mDefinedInClass, mSourceFile, mSourceLine,
                mDisabled, mLoggingOnly, mEnableAfterTargetSdk);
      }

      Builder name(String name) {
        mName = name;
        return this;
      }

      Builder id(long id) {
        mId = id;
        return this;
      }

      Builder description(String description) {
        mDescription = description;
        return this;
      }

      Builder definedInClass(String definedInClass) {
        mDefinedInClass = definedInClass;
        return this;
      }

      Builder sourcePosition(String sourcePosition) throws SAXException {
        if (sourcePosition != null) {
          int colonPos = sourcePosition.indexOf(":");
          if (colonPos == -1) {
            throw new SAXException("Invalid source position: " + sourcePosition);
          }
          mSourceFile = sourcePosition.substring(0, colonPos);
          try {
            mSourceLine = Integer.parseInt(sourcePosition.substring(colonPos + 1));
          } catch (NumberFormatException nfe) {
            throw new SAXException("Invalid source position: " + sourcePosition, nfe);
          }
        }
        return this;
      }

      boolean parseBool(String value) {
        if (value == null) {
          return false;
        }
        boolean result = Boolean.parseBoolean(value);
        return result;
      }

      Builder disabled(String disabled) {
        mDisabled = parseBool(disabled);
        return this;
      }

      Builder loggingOnly(String loggingOnly) {
        mLoggingOnly = parseBool(loggingOnly);
        return this;
      }

      Builder enableAfterTargetSdk(String enableAfter) throws SAXException {
        if (enableAfter == null) {
          mEnableAfterTargetSdk = 0;
        } else {
          try {
            mEnableAfterTargetSdk = Integer.parseInt(enableAfter);
          } catch (NumberFormatException nfe) {
            throw new SAXException("Invalid SDK version int: " + enableAfter, nfe);
          }
        }
        return this;
      }
    }

  }

  private class CompatConfigXmlParser extends DefaultHandler {
    @Override
    public void startElement(String uri, String localName, String qName, Attributes attributes)
        throws SAXException {
      if (qName.equals("compat-change")) {
        mCurrentChange = new CompatChange.Builder();
        String idStr = attributes.getValue("id");
        if (idStr == null) {
          throw new SAXException("<compat-change> element has no id");
        }
        try {
          mCurrentChange.id(Long.parseLong(idStr));
        } catch (NumberFormatException nfe) {
          throw new SAXException("<compat-change> id is not a valid long", nfe);
        }
        mCurrentChange.name(attributes.getValue("name"))
                .description(attributes.getValue("description"))
                .enableAfterTargetSdk(attributes.getValue("enableAfterTargetSdk"))
                .disabled(attributes.getValue("disabled"))
                .loggingOnly(attributes.getValue("loggingOnly"));

      } else if (qName.equals("meta-data")) {
        if (mCurrentChange == null) {
          throw new SAXException("<meta-data> tag with no enclosing <compat-change>");
        }
        mCurrentChange.definedInClass(attributes.getValue("definedIn"))
            .sourcePosition(attributes.getValue("sourcePosition"));
      }
    }

    @Override
    public void endElement(String uri, String localName, String qName) {
      if (qName.equals("compat-change")) {
        mChanges.add(mCurrentChange.build());
        mCurrentChange = null;
      }
    }
  }

  public static CompatInfo readCompatConfig(String source) {
    CompatInfo config = new CompatInfo();
    try {
      InputStream in = new FileInputStream(new File(source));

      XMLReader xmlreader = XMLReaderFactory.createXMLReader();
      xmlreader.setContentHandler(config.mXmlParser);
      xmlreader.setErrorHandler(config.mXmlParser);
      xmlreader.parse(new InputSource(in));
      in.close();
      return config;
    } catch (SAXException e) {
      throw new RuntimeException("Failed to parse " + source, e);
    } catch (IOException e) {
      throw new RuntimeException("Failed to read " + source, e);
    }
  }

  private final CompatConfigXmlParser mXmlParser = new CompatConfigXmlParser();
  private CompatChange.Builder mCurrentChange;
  private List<CompatChange> mChanges = new ArrayList<>();

  public List<CompatChange> getChanges() {
    return mChanges;
  }

  public void makeHDF(Data hdf) {
    // We construct a Comment for each compat change to re-use the default docs generation support
    // for comments.
    mChanges.sort(Comparator.comparing(a -> a.name));
    for (int i = 0; i < mChanges.size(); ++i) {
      CompatInfo.CompatChange change = mChanges.get(i);
      // we will get null ClassInfo here if the defining class is not in the SDK.
      ContainerInfo definedInContainer = Converter.obtainClass(change.definedInClass);
      if (definedInContainer == null) {
        // This happens when the class defining the @ChangeId constant is not included in
        // the sources that the SDK docs are generated from. Using package "android" as the
        // container works, but means we lose the context of the original javadoc comment.
        // This means that if the javadoc comment refers to classes imported by it's
        // containing source file, we cannot resolve those imports here.
        // TODO see if we could somehow plumb the import list from the original source file,
        // via compat_config.xml, so we can resolve links properly here?
        definedInContainer = Converter.obtainPackage("android");
      }
      if (change.description == null) {
        throw new RuntimeException("No desriprion found for @ChangeId " + change.name);
      }
      Comment comment = new Comment(change.description, definedInContainer, new SourcePositionInfo(
          change.sourceFile, change.sourceLine, 1));
      String path = "change." + i;
      hdf.setValue(path + ".id", Long.toString(change.id));
      hdf.setValue(path + ".name", change.name);
      if (change.enableAfterTargetSdk != 0) {
        hdf.setValue(path + ".enableAfterTargetSdk",
            Integer.toString(change.enableAfterTargetSdk));
      }
      if (change.loggingOnly) {
        hdf.setValue(path + ".loggingOnly", Boolean.toString(true));
      }
      if (change.disabled) {
        hdf.setValue(path + ".disabled", Boolean.toString(true));
      }
      TagInfo.makeHDF(hdf, path + ".descr", comment.tags());
    }
  }
}
