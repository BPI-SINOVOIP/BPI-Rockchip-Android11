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

package com.android.cts.helpers;

/** Helper methods for interacting with the print system UI. */
public interface ICtsPrintHelper extends ICtsDeviceInteractionHelper {

    /**
     * Submit a print job based on the current settings.
     *
     * <p>This is the equivalent of the user clicking the print button; the test app must have
     * already called PrintManager.print() to create the print job and start the UI.
     */
    void submitPrintJob();

    /**
     * Retry a failed print job with the current settings.
     *
     * <p>This is the equivalent of the user clicking the "Retry" action button in the
     * print error fragment.
     */
    void retryPrintJob();

    /**
     * Check if the print job can be submitted.
     *
     * <p>A print job can be submitted when a valid printer is selected and all of the options
     * contain valid values.
     *
     * @return true if the print button is enabled, false if not.
     */
    boolean canSubmitJob();

    /**
     * Respond to the first-time dialog when printing to a new service.
     *
     * @param confirm true to accept printing, or false to cancel.
     */
    void answerPrintServicesWarning(boolean confirm);

    /**
     * Select a specific printer from the list of available printers.
     *
     * @param printerName name of the printer
     * @param timeout timeout in milliseconds
     */
    void selectPrinter(String printerName, long timeout);

    /**
     * Wait for a printer to appear in the list of printers, then select it.
     *
     * <p>This is similar to {@link #selectPrinter(String, long)}, except it retries until the
     * printer exists.  Can be used if a printer has just been added and might not yet be available.
     *
     * @param printerName name of the printer
     */
    void selectPrinterWhenAvailable(String printerName);

    /**
     * Set the page orientation to portrait or landscape.
     *
     * @param orientation "Portrait" or "Landscape"
     */
    void setPageOrientation(String orientation);

    /**
     * Get the current page orientation.
     *
     * @return current orientation as "Portrait" or "Landscape"
     */
    String getPageOrientation();

    /**
     * Set the media size.
     *
     * @param mediaSize human-readable label matching one of the PrintAttributes.MediaSize values
     */
    void setMediaSize(String mediaSize);

    /**
     * Set the color mode to Color or Black &amp; White.
     *
     * @param color "Color" or "Black &amp; White"
     */
    void setColorMode(String color);

    /**
     * Get the current color mode.
     *
     * @return "Color" or "Black &amp; White"
     */
    String getColorMode();

    /**
     * Set the duplex mode.
     *
     * @param duplex human-readable label matching one of the DUPLEX_MODE constants from
     *               PrintAttributes
     */
    void setDuplexMode(String duplex);

    /**
     * Set the number of copies to print.
     *
     * @param copies the new number of copies
     */
    void setCopies(int copies);

    /**
     * Get the current number of copies to print.
     *
     * @return the current number of copies
     */
    int getCopies();

    /**
     * Set the page range to print.
     *
     * @param pages a range to print, e.g. "1", "1, 3"
     * @param expectedPages the total number of pages included in the range
     */
    void setPageRange(String pages, int expectedPages);

    /**
     * Get the current page range.
     *
     * <p>If the full page range is selected, "All N" will be returned, where N is the number of
     * pages passed in docPages.  Otherwise, the user's entered range string will be returned.
     *
     * @param docPages the total number of pages in the doc.
     *
     * @return the current page range, e.g. "All 3", "1-2", "1, 3"
     */
    String getPageRange(int docPages);

    /**
     * Get the current status message.
     *
     * @return the text content of the current status message
     */
    String getStatusMessage();

    /**
     * Ensure the basic print options are visible.
     *
     * <p>This is the equivalent of clicking the chevron to expose the basic print options.
     */
    void openPrintOptions();

    /**
     * Ensure the custom print options are visible.
     *
     * <p>This is the equivalent of clicking the "more options" button to open the print service's
     * custom print options.  openPrintOptions() must have been called first to expose the basic
     * print options.
     */
    void openCustomPrintOptions();

    /**
     * Display the full list of printers.
     *
     * <p>This is not intended for selecting a printer; use {@link #selectPrinter(String, long)} for
     * that.  The purpose of displaying the whole list is to ensure that all printers can be
     * successfully enumerated and have their properties retrieved.
     */
    void displayPrinterList();

    /**
     * Display the "more info" activity for the currently selected printer.
     */
    void displayMoreInfo();

    /**
     * Close the printer list that was previously opened with {@link #displayPrinterList()}.
     */
    void closePrinterList();

    /**
     * Close the custom print options that were previously opened with
     * {@link #openCustomPrintOptions()}.
     *
     * This call must be properly nested with other calls to openPrintOptions/closePrintOptions and
     * print/cancelPrinting.
     */
    void closeCustomPrintOptions();

    /**
     * Close the basic print options that were previously opened with {@link #openPrintOptions()}.
     *
     * This call must be properly nested with other calls to
     * openCustomPrintOptions/closeCustomPrintOptions and print/cancelPrinting.
     */
    void closePrintOptions();

    /**
     * Close the main print UI that as previously opened with {@link PrintManager.print()}.
     *
     * This call must be properly nested with other calls to
     * openCustomPrintOptions/closeCustomPrintOptions and openPrintOptions/closePrintOptions.
     */
    void cancelPrinting();
}
