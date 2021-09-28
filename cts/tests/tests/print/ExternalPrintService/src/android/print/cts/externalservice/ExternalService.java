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

package android.print.cts.externalservice;

import static android.print.PrinterInfo.STATUS_IDLE;

import static org.junit.Assert.fail;

import android.print.PrinterId;
import android.print.PrinterInfo;
import android.printservice.PrintJob;
import android.printservice.PrintService;
import android.printservice.PrinterDiscoverySession;

import java.util.Collections;
import java.util.List;

public class ExternalService extends PrintService {
    private static final String PRINTER_NAME = "ExternalServicePrinter";

    @Override
    protected PrinterDiscoverySession onCreatePrinterDiscoverySession() {
        return new PrinterDiscoverySession() {
            @Override
            public void onStartPrinterDiscovery(List<PrinterId> priorityList) {
                addPrinters(Collections.singletonList(
                        (new PrinterInfo.Builder(generatePrinterId(PRINTER_NAME), PRINTER_NAME,
                                STATUS_IDLE)).build()));
            }

            @Override
            public void onStopPrinterDiscovery() {
                // empty
            }

            @Override
            public void onValidatePrinters(List<PrinterId> printerIds) {
                // empty
            }

            @Override
            public void onStartPrinterStateTracking(PrinterId printerId) {
                // empty
            }

            @Override
            public void onStopPrinterStateTracking(PrinterId printerId) {
                // empty
            }

            @Override
            public void onDestroy() {
                // empty
            }
        };
    }

    @Override
    protected void onRequestCancelPrintJob(PrintJob printJob) {
        fail("This service does not support printing");
    }

    @Override
    protected void onPrintJobQueued(PrintJob printJob) {
        fail("This service does not support printing");
    }
}
