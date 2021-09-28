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

package android.print.cts;

import static android.print.PrintAttributes.COLOR_MODE_COLOR;
import static android.print.PrintAttributes.MediaSize.ISO_A5;
import static android.print.PrinterInfo.STATUS_IDLE;
import static android.print.PrinterInfo.STATUS_UNAVAILABLE;
import static android.print.test.Utils.runOnMainThread;

import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintAttributes.Margins;
import android.print.PrintAttributes.Resolution;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentAdapter.LayoutResultCallback;
import android.print.PrintDocumentAdapter.WriteResultCallback;
import android.print.PrintDocumentInfo;
import android.print.PrinterCapabilitiesInfo;
import android.print.PrinterId;
import android.print.PrinterInfo;
import android.print.test.BasePrintTest;
import android.print.test.services.FirstPrintService;
import android.print.test.services.PrinterDiscoverySessionCallbacks;
import android.print.test.services.SecondPrintService;
import android.print.test.services.StubbablePrinterDiscoverySession;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests interaction between printer discovery and print document lifecycle
 */
@RunWith(AndroidJUnit4.class)
public class InteractionBetweenPrintDocumentAndPrinterDiscovery extends BasePrintTest {
    static final String PRINTER_NAME = "Test printer";

    @Before
    public void clearPrintSpoolerState() throws Exception {
        clearPrintSpoolerData();
    }

    /**
     * Add or update the test printer.
     *
     * @param session The printer discovery session the printer belongs to
     * @param status  the status of the printer (idle, unavailable, busy)
     */
    private static void addPrinter(StubbablePrinterDiscoverySession session, int status) {
        List<PrinterInfo> printers = new ArrayList<>();

        PrinterId testId = session.getService().generatePrinterId(PRINTER_NAME);
        PrinterCapabilitiesInfo testCap =
                new PrinterCapabilitiesInfo.Builder(testId)
                        .setMinMargins(new Margins(0, 0, 0, 0))
                        .addMediaSize(ISO_A5, true)
                        .addResolution(new Resolution("r", "r", 1, 1), true)
                        .setColorModes(COLOR_MODE_COLOR, COLOR_MODE_COLOR)
                        .build();
        PrinterInfo testPrinter = new PrinterInfo.Builder(testId, PRINTER_NAME, status)
                .setCapabilities(testCap).build();
        printers.add(testPrinter);

        session.addPrinters(printers);
    }

    /**
     * While a print document adapter write task is getting canceled, make the printer unavailable
     * and available again.
     */
    @Test
    public void printerReappearsWhileCanceling() throws Throwable {
        // The session once initialized
        StubbablePrinterDiscoverySession[] session = new StubbablePrinterDiscoverySession[1];

        // Set up discovery session
        final PrinterDiscoverySessionCallbacks callbacks =
                createMockPrinterDiscoverySessionCallbacks(inv -> {
                    session[0] = ((PrinterDiscoverySessionCallbacks) inv.getMock()).getSession();

                    addPrinter(session[0], STATUS_IDLE);
                    return null;
                }, null, null, null, null, null, invocation -> {
                    onPrinterDiscoverySessionDestroyCalled();
                    return null;
                });

        // Set up print services
        FirstPrintService.setCallbacks(createMockPrintServiceCallbacks((inv) -> callbacks, null,
                null));
        SecondPrintService.setCallbacks(createMockPrintServiceCallbacks(null, null, null));

        // The print attributes set in the layout task
        PrintAttributes[] printAttributes = new PrintAttributes[1];

        // The callback for the last write task
        WriteResultCallback[] writeCallBack = new WriteResultCallback[1];

        // The adapter that will handle the print tasks
        final PrintDocumentAdapter adapter = createMockPrintDocumentAdapter(
                invocation -> {
                    Object[] args = invocation.getArguments();

                    printAttributes[0] = (PrintAttributes) args[1];
                    LayoutResultCallback callback = (LayoutResultCallback) args[3];
                    PrintDocumentInfo info = new PrintDocumentInfo.Builder(PRINT_JOB_NAME).build();
                    callback.onLayoutFinished(info, true);

                    onLayoutCalled();
                    return null;
                }, invocation -> {
                    Object[] args = invocation.getArguments();

                    ((CancellationSignal) args[2]).setOnCancelListener(this::onWriteCancelCalled);

                    // Write single page document
                    ParcelFileDescriptor fd = (ParcelFileDescriptor) args[1];
                    writeBlankPages(printAttributes[0], fd, 0, 1);
                    fd.close();

                    writeCallBack[0] = (WriteResultCallback) args[3];

                    onWriteCalled();
                    return null;
                }, null);

        // Start printing
        print(adapter);

        // Handle write for default printer
        waitForWriteAdapterCallback(1);
        writeCallBack[0].onWriteFinished(new PageRange[]{new PageRange(0, 1)});

        // Select test printer
        selectPrinter(PRINTER_NAME);

        // Selecting the printer changes the document size to A5 which causes a new layout + write
        // Do _not_ call back from the write immediately
        waitForLayoutAdapterCallbackCount(2);
        waitForWriteAdapterCallback(2);

        // While write task is in progress, make the printer unavailable
        runOnMainThread(() -> addPrinter(session[0], STATUS_UNAVAILABLE));

        // This should cancel the write task
        waitForWriteCancelCallback(1);

        // Make the printer available again, this should add a new task that re-tries the canceled
        // task
        runOnMainThread(() -> addPrinter(session[0], STATUS_IDLE));

        // Give print preview UI some time to schedule new write task
        Thread.sleep(500);

        // Report write from above as canceled
        writeCallBack[0].onWriteCancelled();

        // Now the write that was canceled before should have been retried
        waitForWriteAdapterCallback(3);

        // Finish test: Fail pending write, exit print activity and wait for print subsystem to shut
        // down
        writeCallBack[0].onWriteFailed(null);
        mPrintHelper.cancelPrinting();
        waitForPrinterDiscoverySessionDestroyCallbackCalled(1);
    }
}
