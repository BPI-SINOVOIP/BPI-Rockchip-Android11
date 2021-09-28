# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import dbus
import gzip
import logging
import os
import subprocess
import shutil
import tempfile

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import file_utils
from autotest_lib.client.cros import debugd_util

import archiver
import configurator
import helpers
import fake_printer
import log_reader
import multithreaded_processor

# Timeout for printing documents in seconds
_FAKE_PRINTER_TIMEOUT = 200

# Prefix for CUPS printer name
_FAKE_PRINTER_ID = 'FakePrinter'

# First port number to use, this test uses consecutive ports numbers,
# different for every PPD file
_FIRST_PORT_NUMBER = 9000

# Values are from platform/system_api/dbus/debugd/dbus-constants.h.
_CUPS_SUCCESS = 0

# Exceptions, cases that we want to omit/ignore
# key: document; values: list of PPD files
_EXCEPTIONS = { 'split_streams.pdf': ['HP-DeskJet_200-pcl3.ppd.gz',
        'HP-DeskJet_310-pcl3.ppd.gz', 'HP-DeskJet_320-pcl3.ppd.gz',
        'HP-DeskJet_340C-pcl3.ppd.gz', 'HP-DeskJet_540C-pcl3.ppd.gz',
        'HP-DeskJet_560C-pcl3.ppd.gz'] }

class platform_PrinterPpds(test.test):
    """
    This test gets a list of PPD files and a list of test documents. It tries
    to add printer using each PPD file and to print all test documents on
    every printer created this way. Becasue the number of PPD files to test can
    be large (more then 3K), PPD files are tested simultaneously in many
    threads.

    """
    version = 3


    def _get_filenames_from_PPD_indexes(self):
        """
        It returns all PPD filenames from SCS server.

        @returns a list of PPD filenames without duplicates

        """
        # extracts PPD filenames from all 20 index files (in parallel)
        outputs = self._processor.run(helpers.get_filenames_from_PPD_index, 20)
        # joins obtained lists and performs deduplication
        ppd_files = set()
        for output in outputs:
            ppd_files.update(output)
        return list(ppd_files)


    def _calculate_full_path(self, path):
        """
        Converts path given as a parameter to absolute path.

        @param path: a path set in configuration (relative, absolute or None)

        @returns absolute path or None if the input parameter was None

        """
        if path is None or os.path.isabs(path):
            return path
        path_current = os.path.dirname(os.path.realpath(__file__))
        return os.path.join(path_current, path)


    def initialize(
            self, path_docs, path_ppds=None, path_digests=None,
            debug_mode=False, threads_count=8):
        """
        @param path_docs: path to local directory with documents to print
        @param path_ppds: path to local directory with PPD files to test;
                if None is set then all PPD files from the SCS server are
                downloaded and tested
        @param path_digests: path to local directory with digests files for
                test documents; if None is set then content of printed
                documents is not verified
        @param debug_mode: if set to True, then the autotest temporarily
                remounts the root partition in R/W mode and changes CUPS
                configuration, what allows to extract pipelines for all tested
                PPDs and rerun the outside CUPS
        @param threads_count: number of threads to use

        """
        # Calculates absolute paths for all parameters
        self._location_of_test_docs = self._calculate_full_path(path_docs)
        self._location_of_PPD_files = self._calculate_full_path(path_ppds)
        location_of_digests_files = self._calculate_full_path(path_digests)

        # This object is used for running tasks in many threads simultaneously
        self._processor = multithreaded_processor.MultithreadedProcessor(
                threads_count)

        # This object is responsible for parsing CUPS logs
        self._log_reader = log_reader.LogReader()

        # This object is responsible for the system configuration
        self._configurator = configurator.Configurator()
        self._configurator.configure(debug_mode)

        # Reads list of test documents
        self._docs = helpers.list_entries_from_directory(
                            path=self._location_of_test_docs,
                            with_suffixes=('.pdf'),
                            nonempty_results=True,
                            include_directories=False)

        # Get list of PPD files ...
        if self._location_of_PPD_files is None:
            # ... from the SCS server
            self._ppds = self._get_filenames_from_PPD_indexes()
        else:
            # ... from the given local directory
            # Unpack archives with all PPD files:
            path_archive = self._calculate_full_path('ppds_all.tar.xz')
            path_target_dir = self._calculate_full_path('.')
            file_utils.rm_dir_if_exists(
                    os.path.join(path_target_dir,'ppds_all'))
            subprocess.call(['tar', 'xJf', path_archive, '-C', path_target_dir])
            path_archive = self._calculate_full_path('ppds_100.tar.xz')
            file_utils.rm_dir_if_exists(
                    os.path.join(path_target_dir,'ppds_100'))
            subprocess.call(['tar', 'xJf', path_archive, '-C', path_target_dir])
            # Load PPD files from the chosen directory
            self._ppds = helpers.list_entries_from_directory(
                            path=self._location_of_PPD_files,
                            with_suffixes=('.ppd','.ppd.gz'),
                            nonempty_results=True,
                            include_directories=False)
        self._ppds.sort()

        # Load digests files
        self._digests = dict()
        if location_of_digests_files is None:
            for doc_name in self._docs:
                self._digests[doc_name] = dict()
        else:
            path_blacklist = os.path.join(location_of_digests_files,
                    'blacklist.txt')
            blacklist = helpers.load_blacklist(path_blacklist)
            for doc_name in self._docs:
                digests_name = doc_name + '.digests'
                path = os.path.join(location_of_digests_files, digests_name)
                self._digests[doc_name] = helpers.parse_digests_file(path,
                        blacklist)

        # Prepare a working directory for pipelines
        if debug_mode:
            self._pipeline_dir = tempfile.mkdtemp(dir='/tmp')
        else:
            self._pipeline_dir = None


    def cleanup(self):
        """
        Cleanup.

        """
        # Resore previous system settings
        self._configurator.restore()

        # Delete directories with PPD files
        path_ppds = self._calculate_full_path('ppds_100')
        file_utils.rm_dir_if_exists(path_ppds)
        path_ppds = self._calculate_full_path('ppds_all')
        file_utils.rm_dir_if_exists(path_ppds)

        # Delete pipeline working directory
        if self._pipeline_dir is not None:
            file_utils.rm_dir_if_exists(self._pipeline_dir)


    def run_once(self, path_outputs=None):
        """
        This is the main test function. It runs the testing procedure for
        every PPD file. Tests are run simultaneously in many threads.

        @param path_outputs: if it is not None, raw outputs sent
                to printers are dumped here; the directory is overwritten if
                already exists (is deleted and recreated)

        @raises error.TestFail if at least one of the tests failed

        """
        # Set directory for output documents
        self._path_output_directory = self._calculate_full_path(path_outputs)
        if self._path_output_directory is not None:
            # Delete whole directory if already exists
            file_utils.rm_dir_if_exists(self._path_output_directory)
            # Create archivers
            self._archivers = dict()
            for doc_name in self._docs:
                path_for_archiver = os.path.join(self._path_output_directory,
                        doc_name)
                self._archivers[doc_name] = archiver.Archiver(path_for_archiver,
                        self._ppds, 50)
            # A place for new digests
            self._new_digests = dict()
            for doc_name in self._docs:
                self._new_digests[doc_name] = dict()

        # Runs tests for all PPD files (in parallel)
        outputs = self._processor.run(self._thread_test_PPD, len(self._ppds))

        # Analyses tests' outputs, prints a summary report and builds a list
        # of PPD filenames that failed
        failures = []
        for i, output in enumerate(outputs):
            ppd_file = self._ppds[i]
            if output != True:
                failures.append(ppd_file)
            else:
                output = 'OK'
            line = "%s: %s" % (ppd_file, output)
            logging.info(line)

        # Calculate digests files for output documents (if dumped)
        if self._path_output_directory is not None:
            for doc_name in self._docs:
                path = os.path.join(self._path_output_directory,
                        doc_name + '.digests')
                helpers.save_digests_file(path, self._new_digests[doc_name],
                        failures)

        # Raises an exception if at least one test failed
        if len(failures) > 0:
            failures.sort()
            raise error.TestFail(
                    'Test failed for %d PPD files: %s'
                    % (len(failures), ', '.join(failures)) )


    def _thread_test_PPD(self, task_id):
        """
        Runs a test procedure for single PPD file.

        It retrieves assigned PPD file and run for it a test procedure.

        @param task_id: an index of the PPD file in self._ppds

        @returns True when the test was passed or description of the error
                (string) if the test failed

        """
        # Gets content of the PPD file
        try:
            ppd_file = self._ppds[task_id]
            if self._location_of_PPD_files is None:
                # Downloads PPD file from the SCS server
                ppd_content = helpers.download_PPD_file(ppd_file)
            else:
                # Reads PPD file from local filesystem
                path_ppd = os.path.join(self._location_of_PPD_files, ppd_file)
                with open(path_ppd, 'rb') as ppd_file_descriptor:
                    ppd_content = ppd_file_descriptor.read()
        except BaseException as e:
            return 'MISSING PPD: ' + str(e)

        # Runs the test procedure
        try:
            port = _FIRST_PORT_NUMBER + task_id
            self._PPD_test_procedure(ppd_file, ppd_content, port)
        except BaseException as e:
            return 'FAIL: ' + str(e)

        return True


    def _PPD_test_procedure(self, ppd_name, ppd_content, port):
        """
        Test procedure for single PPD file.

        It tries to run the following steps:
        1. Starts an instance of FakePrinter
        2. Configures CUPS printer
        3. For each test document run the following steps:
            3a. Sends tests documents to the CUPS printer
            3b. Fetches the raw document from the FakePrinter
            3c. Parse CUPS logs and check for any errors
            3d. If self._pipeline_dir is set, extract the executed CUPS
                pipeline, rerun it in bash console and verify every step and
                final output
            3e. If self._path_output_directory is set, save the raw document
                and all intermediate steps in the provided directory
            3f. If the digest is available, verify a digest of an output
                documents
        4. Removes CUPS printer and stops FakePrinter
        If the test fails this method throws an exception.

        @param ppd_name: a name of the PPD file
        @param ppd_content: a content of the PPD file
        @param port: a port for the printer

        @throws Exception when the test fails

        """
        # Create work directory for external pipelines and save the PPD file
        # there (if needed)
        path_ppd = None
        if self._pipeline_dir is not None:
            path_pipeline_ppd_dir = os.path.join(self._pipeline_dir, ppd_name)
            os.makedirs(path_pipeline_ppd_dir)
            path_ppd = os.path.join(path_pipeline_ppd_dir, ppd_name)
            with open(path_ppd, 'wb') as file_ppd:
                file_ppd.write(ppd_content)
            if path_ppd.endswith('.gz'):
                subprocess.call(['gzip', '-d', path_ppd])
                path_ppd = path_ppd[0:-3]

        try:
            # Starts the fake printer
            with fake_printer.FakePrinter(port) as printer:

                # Add a CUPS printer manually with given ppd file
                cups_printer_id = '%s_at_%05d' % (_FAKE_PRINTER_ID,port)
                result = debugd_util.iface().CupsAddManuallyConfiguredPrinter(
                                             cups_printer_id,
                                             'socket://127.0.0.1:%d' % port,
                                             dbus.ByteArray(ppd_content))
                if result != _CUPS_SUCCESS:
                    raise Exception('valid_config - Could not setup valid '
                        'printer %d' % result)

                # Prints all test documents
                try:
                    for doc_name in self._docs:
                        # Omit exceptions
                        if ( doc_name in _EXCEPTIONS and
                                ppd_name in _EXCEPTIONS[doc_name] ):
                            if self._path_output_directory is not None:
                                self._new_digests[doc_name][ppd_name] = (
                                        helpers.calculate_digest('\x00') )
                            continue
                        # Full path to the test document
                        path_doc = os.path.join(
                                        self._location_of_test_docs, doc_name)
                        # Sends test document to printer
                        argv = ['lp', '-d', cups_printer_id]
                        argv += [path_doc]
                        subprocess.call(argv)
                        # Prepare a workdir for the pipeline (if needed)
                        path_pipeline_workdir_temp = None
                        if self._pipeline_dir is not None:
                            path_pipeline_workdir = os.path.join(
                                    path_pipeline_ppd_dir, doc_name)
                            path_pipeline_workdir_temp = os.path.join(
                                    path_pipeline_workdir, 'temp')
                            os.makedirs(path_pipeline_workdir_temp)
                        # Gets the output document from the fake printer
                        doc = printer.fetch_document(_FAKE_PRINTER_TIMEOUT)
                        digest = helpers.calculate_digest(doc)
                        # Retrive data from the log file
                        no_errors, logs, pipeline = \
                                self._log_reader.extract_result(
                                        cups_printer_id, path_ppd, path_doc,
                                        path_pipeline_workdir_temp)
                        # Archive obtained results in the output directory
                        if self._path_output_directory is not None:
                            self._archivers[doc_name].save_file(
                                    ppd_name, '.out', doc, apply_gzip=True)
                            self._archivers[doc_name].save_file(
                                    ppd_name, '.log', logs)
                            if pipeline is not None:
                                self._archivers[doc_name].save_file(
                                        ppd_name, '.sh', pipeline)
                            # Set new digest
                            self._new_digests[doc_name][ppd_name] = digest
                        # Fail if any of CUPS filters failed
                        if not no_errors:
                            raise Exception('One of the CUPS filters failed')
                        # Reruns the pipeline and dump intermediate outputs
                        if self._pipeline_dir is not None:
                            self._rerun_whole_pipeline(
                                        pipeline, path_pipeline_workdir,
                                        ppd_name, doc_name, digest)
                            shutil.rmtree(path_pipeline_workdir)
                        # Check document's digest (if known)
                        if ppd_name in self._digests[doc_name]:
                            digest_expected = self._digests[doc_name][ppd_name]
                            if digest_expected != digest:
                                message = 'Document\'s digest does not match'
                                raise Exception(message)
                        else:
                            # Simple validation
                            if len(doc) < 16:
                                raise Exception('Empty output')
                finally:
                    # Remove CUPS printer
                    debugd_util.iface().CupsRemovePrinter(cups_printer_id)

            # The fake printer is stopped at the end of "with" statement
        finally:
            # Finalize archivers and cleaning
            if self._path_output_directory is not None:
                for doc_name in self._docs:
                    self._archivers[doc_name].finalize_prefix(ppd_name)
            # Clean the pipelines' working directories
            if self._pipeline_dir is not None:
                shutil.rmtree(path_pipeline_ppd_dir)


    def _rerun_whole_pipeline(
            self, pipeline, path_workdir, ppd_name, doc_name, digest):
        """
        Reruns the whole pipeline outside CUPS server.

        Reruns a printing pipeline dumped from CUPS. All intermediate outputs
        are dumped and archived for future analysis.

        @param pipeline: a pipeline as a bash script
        @param path_workdir: an existing directory to use as working directory
        @param ppd_name: a filenames prefix used for archivers
        @param doc_name: a document name, used to select a proper archiver
        @param digest: an digest of the output produced by CUPS (for comparison)

        @raises Exception in case of any errors

        """
        # Save pipeline to a file
        path_pipeline = os.path.join(path_workdir, 'pipeline.sh')
        with open(path_pipeline, 'wb') as file_pipeline:
            file_pipeline.write(pipeline)
        # Run the pipeline
        argv = ['/bin/bash', '-e', path_pipeline]
        ret = subprocess.Popen(argv, cwd=path_workdir).wait()
        # Find the number of output files
        i = 1
        while os.path.isfile(os.path.join(path_workdir, "%d.doc.gz" % i)):
            i += 1
        files_count = i-1
        # Reads the last output (to compare it with the output produced by CUPS)
        if ret == 0:
            with gzip.open(os.path.join(path_workdir,
                    "%d.doc.gz" % files_count)) as last_file:
                content_digest = helpers.calculate_digest(last_file.read())
        # Archives all intermediate files (if desired)
        if self._path_output_directory is not None:
            for i in range(1,files_count+1):
                self._archivers[doc_name].move_file(ppd_name, ".err%d" % i,
                        os.path.join(path_workdir, "%d.err" % i))
                self._archivers[doc_name].move_file(ppd_name, ".out%d.gz" % i,
                        os.path.join(path_workdir, "%d.doc.gz" % i))
        # Validation
        if ret != 0:
            raise Exception("A pipeline script returned %d" % ret)
        if content_digest != digest:
            raise Exception("The output returned by the pipeline is different"
                    " than the output produced by CUPS")
