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

package com.android.cts.helpers.aosp;

import android.app.Instrumentation;
import android.app.UiAutomation;
import android.os.RemoteException;

import android.platform.helpers.exceptions.TestHelperException;
import android.support.test.uiautomator.By;
import android.support.test.uiautomator.UiDevice;
import android.support.test.uiautomator.UiObject;
import android.support.test.uiautomator.UiObject2;
import android.support.test.uiautomator.UiObjectNotFoundException;
import android.support.test.uiautomator.UiSelector;
import android.support.test.uiautomator.Until;
import android.util.Log;

import com.android.cts.helpers.ICtsPrintHelper;

import java.io.ByteArrayOutputStream;
import java.io.IOException;

public class DefaultCtsPrintHelper implements ICtsPrintHelper {
    private static final String LOG_TAG = DefaultCtsPrintHelper.class.getSimpleName();

    protected static final long OPERATION_TIMEOUT_MILLIS = 60000;

    protected Instrumentation mInstrumentation;
    protected UiDevice mDevice;
    protected UiAutomation mAutomation;

    public DefaultCtsPrintHelper(Instrumentation instrumentation) {
        mInstrumentation = instrumentation;
        mDevice = UiDevice.getInstance(mInstrumentation);
        mAutomation = mInstrumentation.getUiAutomation();
    }

    protected void dumpWindowHierarchy() throws TestHelperException {
        try {
            ByteArrayOutputStream os = new ByteArrayOutputStream();
            mDevice.dumpWindowHierarchy(os);

            Log.w(LOG_TAG, "Window hierarchy:");
            for (String line : os.toString("UTF-8").split("\n")) {
                Log.w(LOG_TAG, line);
            }
        } catch (IOException e) {
            throw new TestHelperException(e);
        }
    }

    @Override
    public void setUp() throws TestHelperException {
        try {
            // Prevent rotation
            mDevice.freezeRotation();
            while (!mDevice.isNaturalOrientation()) {
                mDevice.setOrientationNatural();
                mDevice.waitForIdle();
            }
        } catch (RemoteException e) {
            throw new TestHelperException("Failed to freeze device rotation", e);
        }
    }

    @Override
    public void tearDown() throws TestHelperException {
        try {
            // Allow rotation
            mDevice.unfreezeRotation();
        } catch (RemoteException e) {
            throw new TestHelperException("Failed to unfreeze device rotation", e);
        }
    }

    @Override
    public void submitPrintJob() throws TestHelperException {
        Log.d(LOG_TAG, "Clicking print button");

        mDevice.waitForIdle();

        UiObject2 printButton =
                mDevice.wait(
                        Until.findObject(By.res("com.android.printspooler:id/print_button")),
                        OPERATION_TIMEOUT_MILLIS);
        if (printButton == null) {
            dumpWindowHierarchy();
            throw new TestHelperException("print button not found");
        }

        printButton.click();
    }

    @Override
    public void retryPrintJob() throws TestHelperException {
        try {
            UiObject retryButton = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/action_button"));
            retryButton.click();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Retry button not found", e);
        }
    }

    @Override
    public boolean canSubmitJob() {
        return mDevice.hasObject(By.res("com.android.printspooler:id/print_button"));
    }

    @Override
    public void answerPrintServicesWarning(boolean confirm) throws TestHelperException {
        try {
            mDevice.waitForIdle();
            UiObject button;
            if (confirm) {
                button = mDevice.findObject(new UiSelector().resourceId("android:id/button1"));
            } else {
                button = mDevice.findObject(new UiSelector().resourceId("android:id/button2"));
            }
            button.click();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to find print service dialog button", e);
        }
    }

    @Override
    public void selectPrinter(String printerName, long timeout) throws TestHelperException {
        Log.d(LOG_TAG, "Selecting printer " + printerName);
        try {
            UiObject2 destinationSpinner =
                    mDevice.wait(
                            Until.findObject(
                                    By.res("com.android.printspooler:id/destination_spinner")),
                            timeout);

            if (destinationSpinner != null) {
                destinationSpinner.click();
                mDevice.waitForIdle();
            }

            UiObject2 printerOption = mDevice.wait(Until.findObject(By.text(printerName)), timeout);
            if (printerOption == null) {
                throw new UiObjectNotFoundException(printerName + " not found");
            }

            printerOption.click();
            mDevice.waitForIdle();
        } catch (Exception e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Failed to select printer", e);
        }
    }

    @Override
    public void selectPrinterWhenAvailable(String printerName) throws TestHelperException {
        try {
            while (true) {
                UiObject printerItem = mDevice.findObject(
                        new UiSelector().text(printerName));

                if (printerItem.isEnabled()) {
                    printerItem.click();
                    break;
                } else {
                    Thread.sleep(100);
                }
            }
        } catch (UiObjectNotFoundException e) {
            throw new TestHelperException("Failed to find printer label", e);
        } catch (InterruptedException e) {
            throw new TestHelperException("Interruped while waiting for printer", e);
        }
    }

    @Override
    public void setPageOrientation(String orientation) throws TestHelperException {
        try {
            UiObject orientationSpinner = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/orientation_spinner"));
            orientationSpinner.click();
            UiObject orientationOption = mDevice.findObject(new UiSelector().text(orientation));
            orientationOption.click();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Failed to set page orientation to " + orientation, e);
        }
    }

    @Override
    public String getPageOrientation() throws TestHelperException {
        try {
            UiObject orientationSpinner = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/orientation_spinner"));
            return orientationSpinner.getText();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Failed to get page orientation", e);
        }
    }

    @Override
    public void setMediaSize(String mediaSize) throws TestHelperException {
        try {
            UiObject mediaSizeSpinner = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/paper_size_spinner"));
            mediaSizeSpinner.click();
            UiObject mediaSizeOption = mDevice.findObject(new UiSelector().text(mediaSize));
            mediaSizeOption.click();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to set media size to " + mediaSize, e);
        }
    }

    @Override
    public void setColorMode(String color) throws TestHelperException {
        try {
            UiObject colorSpinner = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/color_spinner"));
            colorSpinner.click();
            UiObject colorOption = mDevice.findObject(new UiSelector().text(color));
            colorOption.click();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to set color mode to " + color, e);
        }
    }

    @Override
    public String getColorMode() throws TestHelperException {
        try {
            UiObject colorSpinner = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/color_spinner"));
            return colorSpinner.getText();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to get color mode", e);
        }
    }

    @Override
    public void setDuplexMode(String duplex) throws TestHelperException {
        try {
            UiObject duplexSpinner = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/duplex_spinner"));
            duplexSpinner.click();
            UiObject duplexOption = mDevice.findObject(new UiSelector().text(duplex));
            duplexOption.click();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to set duplex mode to " + duplex, e);
        }
    }

    @Override
    public void setCopies(int newCopies) throws TestHelperException {
        try {
            UiObject copies = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/copies_edittext"));
            copies.setText(Integer.valueOf(newCopies).toString());
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to set copies to " + newCopies, e);
        }
    }

    @Override
    public int getCopies() throws TestHelperException {
        try {
            UiObject copies = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/copies_edittext"));
            return Integer.parseInt(copies.getText());
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to get number of copies", e);
        }
    }

    @Override
    public void setPageRange(String pages, int expectedPages) throws TestHelperException {
        try {
            mDevice.waitForIdle();
            UiObject pagesSpinner = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/range_options_spinner"));
            pagesSpinner.click();

            mDevice.waitForIdle();
            UiObject rangeOption = mDevice.findObject(new UiSelector().textContains("Range of "
                    + expectedPages));
            rangeOption.click();

            mDevice.waitForIdle();
            UiObject pagesEditText = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/page_range_edittext"));
            pagesEditText.setText(pages);

            mDevice.waitForIdle();
            // Hide the keyboard.
            mDevice.pressBack();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to set page range", e);
        }
    }

    @Override
    public String getPageRange(int docPages) throws TestHelperException {
        final String fullRange = "All " + docPages;

        try {
            if (mDevice.hasObject(By.text(fullRange))) {
                return fullRange;
            }

            UiObject pagesEditText = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/page_range_edittext"));

            return pagesEditText.getText();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to get page range", e);
        }
    }

    @Override
    public String getStatusMessage() throws TestHelperException {
        UiObject2 message = mDevice.wait(Until.findObject(
                By.res("com.android.printspooler:id/message")), OPERATION_TIMEOUT_MILLIS);

        if (message == null) {
            dumpWindowHierarchy();
            throw new TestHelperException("Cannot find status message");
        }

        return message.getText();
    }

    @Override
    public void openPrintOptions() throws TestHelperException {
        try {
            UiObject expandHandle = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/expand_collapse_handle"));
            expandHandle.click();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to find print options handle", e);
        }
    }

    @Override
    public void openCustomPrintOptions() throws TestHelperException {
        try {
            UiObject expandHandle = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/more_options_button"));
            expandHandle.click();
        } catch (UiObjectNotFoundException e) {
            dumpWindowHierarchy();
            throw new TestHelperException("Unable to find print options handle", e);
        }
    }

    @Override
    public void displayPrinterList() throws TestHelperException {
        try {
            // Open destination spinner
            UiObject destinationSpinner = mDevice.findObject(new UiSelector().resourceId(
                    "com.android.printspooler:id/destination_spinner"));
            destinationSpinner.click();

            // Wait until spinner is opened
            mDevice.waitForIdle();
        } catch (UiObjectNotFoundException e) {
            throw new TestHelperException("Unable to find destination spinner", e);
        }
    }

    @Override
    public void displayMoreInfo() throws TestHelperException {
        mDevice.waitForIdle();
        mDevice.wait(
                Until.findObject(By.res("com.android.printspooler:id/more_info")),
                OPERATION_TIMEOUT_MILLIS).click();
    }

    @Override
    public void closePrinterList() {
        mDevice.pressBack();
    }

    @Override
    public void closeCustomPrintOptions() {
        mDevice.pressBack();
    }

    @Override
    public void closePrintOptions() {
        mDevice.pressBack();
    }

    @Override
    public void cancelPrinting() throws TestHelperException {
        try {
            mDevice.wakeUp();
            mDevice.pressBack();
            mDevice.waitForIdle();
        } catch (RemoteException e) {
            throw new TestHelperException("Failed to cancel printing", e);
        }
    }
}
