/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.bluetooth.avrcpcontroller;

import android.util.Log;

import com.android.internal.util.FastXmlSerializer;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;
import org.xmlpull.v1.XmlSerializer;

import java.io.IOException;
import java.io.InputStream;
import java.io.StringWriter;
import java.io.UnsupportedEncodingException;

/**
 * Contains the metadata that describes either (1) the desired size of a image to be downloaded or
 * (2) the extact size of an image to be uploaded.
 *
 * When using this to assert the size of an image to download/pull, it's best to derive this
 * specific descriptor from any of the available BipImageFormat options returned from a
 * RequestGetImageProperties. Note that if a BipImageFormat is not of a fixed size type then you
 * must arrive on a desired fixed size for this descriptor.
 *
 * When using this to denote the size of an image when pushing an image for transfer this descriptor
 * must match the metadata of the image being sent.
 *
 * Note, the encoding and pixel values are mandatory by specification. The version number is fixed.
 * All other values are optional. The transformation field is to have *one* selected option in it.
 *
 * Example:
 *   < image-descriptor version=“1.0” >
 *   < image encoding=“JPEG” pixel=“1280*960” size=“500000”/>
 *   < /image-descriptor >
 */
public class BipImageDescriptor {
    private static final String TAG = "avrcpcontroller.BipImageDescriptor";
    private static final String sVersion = "1.0";

    /**
     * A Builder for an ImageDescriptor object
     */
    public static class Builder {
        private BipImageDescriptor mImageDescriptor = new BipImageDescriptor();
        /**
         * Set the encoding for the descriptor you're building using a BipEncoding object
         *
         * @param encoding The encoding you would like to set
         * @return This object so you can continue building
         */
        public Builder setEncoding(BipEncoding encoding) {
            mImageDescriptor.mEncoding = encoding;
            return this;
        }

        /**
         * Set the encoding for the descriptor you're building using a BipEncoding.* type value
         *
         * @param encoding The encoding you would like to set as a BipEncoding.* type value
         * @return This object so you can continue building
         */
        public Builder setEncoding(int encoding) {
            mImageDescriptor.mEncoding = new BipEncoding(encoding, null);
            return this;
        }

        /**
         * Set the encoding for the descriptor you're building using a BIP defined string name of
         * the encoding you want
         *
         * @param encoding The encoding you would like to set as a BIP spec defined string
         * @return This object so you can continue building
         */
        public Builder setPropietaryEncoding(String encoding) {
            mImageDescriptor.mEncoding = new BipEncoding(BipEncoding.USR_XXX, encoding);
            return this;
        }

        /**
         * Set the fixed X by Y image dimensions for the descriptor you're building
         *
         * @param width The number of pixels in width of the image
         * @param height The number of pixels in height of the image
         * @return This object so you can continue building
         */
        public Builder setFixedDimensions(int width, int height) {
            mImageDescriptor.mPixel = BipPixel.createFixed(width, height);
            return this;
        }

        /**
         * Set the transformation used for the descriptor you're building
         *
         * @param transformation The BipTransformation.* type value of the used transformation
         * @return This object so you can continue building
         */
        public Builder setTransformation(int transformation) {
            mImageDescriptor.mTransformation = new BipTransformation(transformation);
            return this;
        }

        /**
         * Set the image file size for the descriptor you're building
         *
         * @param size The image size in bytes
         * @return This object so you can continue building
         */
        public Builder setFileSize(int size) {
            mImageDescriptor.mSize = size;
            return this;
        }

        /**
         * Set the max file size of the image for the descriptor you're building
         *
         * @param size The maxe image size in bytes
         * @return This object so you can continue building
         */
        public Builder setMaxFileSize(int size) {
            mImageDescriptor.mMaxSize = size;
            return this;
        }

        /**
         * Build the object
         *
         * @return A BipImageDescriptor object
         */
        public BipImageDescriptor build() {
            return mImageDescriptor;
        }
    }

    /**
     * The version of the image-descriptor XML string
     */
    private String mVersion = null;

    /**
     * The encoding of the image, required by the specification
     */
    private BipEncoding mEncoding = null;

    /**
     * The width and height of the image, required by the specification
     */
    private BipPixel mPixel = null;

    /**
     * The transformation to be applied to the image, *one* of BipTransformation.STRETCH,
     * BipTransformation.CROP, or BipTransformation.FILL placed into a BipTransformation object
     */
    private BipTransformation mTransformation = null;

    /**
     * The size in bytes of the image
     */
    private int mSize = -1;

    /**
     * The max size in bytes of the image.
     *
     * Optional, used only when describing an image to pull
     */
    private int mMaxSize = -1;

    private BipImageDescriptor() {
        mVersion = sVersion;
    }

    public BipImageDescriptor(InputStream inputStream) {
        parse(inputStream);
    }

    private void parse(InputStream inputStream) {
        try {
            XmlPullParser xpp = XmlPullParserFactory.newInstance().newPullParser();
            xpp.setInput(inputStream, "utf-8");
            int event = xpp.getEventType();
            while (event != XmlPullParser.END_DOCUMENT) {
                switch (event) {
                    case XmlPullParser.START_TAG:
                        String tag = xpp.getName();
                        if (tag.equals("image-descriptor")) {
                            mVersion = xpp.getAttributeValue(null, "version");
                        } else if (tag.equals("image")) {
                            mEncoding = new BipEncoding(xpp.getAttributeValue(null, "encoding"));
                            mPixel = new BipPixel(xpp.getAttributeValue(null, "pixel"));
                            mSize = parseInt(xpp.getAttributeValue(null, "size"));
                            mMaxSize = parseInt(xpp.getAttributeValue(null, "maxsize"));
                            mTransformation = new BipTransformation(
                                        xpp.getAttributeValue(null, "transformation"));
                        } else {
                            Log.w(TAG, "Unrecognized tag in x-bt/img-Description object: " + tag);
                        }
                        break;
                    case XmlPullParser.END_TAG:
                        break;
                }
                event = xpp.next();
            }
            return;
        } catch (XmlPullParserException e) {
            Log.e(TAG, "XML parser error when parsing XML", e);
        } catch (IOException e) {
            Log.e(TAG, "I/O error when parsing XML", e);
        }
        throw new ParseException("Failed to parse image-descriptor from stream");
    }

    private static int parseInt(String s) {
        if (s == null) return -1;
        try {
            return Integer.parseInt(s);
        } catch (NumberFormatException e) {
            error("Failed to parse '" + s + "'");
        }
        return -1;
    }

    public BipEncoding getEncoding() {
        return mEncoding;
    }

    public BipPixel getPixel() {
        return mPixel;
    }

    public BipTransformation getTransformation() {
        return mTransformation;
    }

    public int getSize() {
        return mSize;
    }

    public int getMaxSize() {
        return mMaxSize;
    }

    /**
     * Serialize this object into a byte array ready for transfer overOBEX
     *
     * @return A byte array containing this object's info, or null on error.
     */
    public byte[] serialize() {
        String s = toString();
        try {
            return s != null ? s.getBytes("UTF-8") : null;
        } catch (UnsupportedEncodingException e) {
            return null;
        }
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;
        if (!(o instanceof BipImageDescriptor)) return false;

        BipImageDescriptor d = (BipImageDescriptor) o;
        return d.getEncoding() == getEncoding()
                && d.getPixel() == getPixel()
                && d.getTransformation() == getTransformation()
                && d.getSize() == getSize()
                && d.getMaxSize() == getMaxSize();
    }

    @Override
    public String toString() {
        if (mEncoding == null || mPixel == null) {
            error("Missing required fields [ " + (mEncoding == null ? "encoding " : "")
                    + (mPixel == null ? "pixel " : ""));
            return null;
        }
        StringWriter writer = new StringWriter();
        XmlSerializer xmlMsgElement = new FastXmlSerializer();
        try {
            xmlMsgElement.setOutput(writer);
            xmlMsgElement.startDocument("UTF-8", true);
            xmlMsgElement.setFeature("http://xmlpull.org/v1/doc/features.html#indent-output", true);
            xmlMsgElement.startTag(null, "image-descriptor");
            xmlMsgElement.attribute(null, "version", sVersion);
            xmlMsgElement.startTag(null, "image");
            xmlMsgElement.attribute(null, "encoding", mEncoding.toString());
            xmlMsgElement.attribute(null, "pixel", mPixel.toString());
            if (mSize != -1) {
                xmlMsgElement.attribute(null, "size", Integer.toString(mSize));
            }
            if (mMaxSize != -1) {
                xmlMsgElement.attribute(null, "maxsize", Integer.toString(mMaxSize));
            }
            if (mTransformation != null && mTransformation.supportsAny()) {
                xmlMsgElement.attribute(null, "transformation", mTransformation.toString());
            }
            xmlMsgElement.endTag(null, "image");
            xmlMsgElement.endTag(null, "image-descriptor");
            xmlMsgElement.endDocument();
            return writer.toString();
        } catch (IllegalArgumentException e) {
            error(e.toString());
        } catch (IllegalStateException e) {
            error(e.toString());
        } catch (IOException e) {
            error(e.toString());
        }
        return null;
    }

    private static void error(String msg) {
        Log.e(TAG, msg);
    }
}
