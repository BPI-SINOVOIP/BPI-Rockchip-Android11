#!/usr/bin/env python
#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the 'License');
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an 'AS IS' BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import os
import tempfile
import unittest
import zipfile

try:
    from unittest import mock
except ImportError:
    # TODO: Remove when we stop supporting Python 2
    import mock

from host_controller import common
from host_controller.command_processor import command_sheet

# Input
_EXTRA_ROWS = ("logs,gs://a/b", "logs,gs://c/{suite_plan}")

_XML_HEAD = """\
<?xml version='1.0' encoding='UTF-8' standalone='no' ?>
<?xml-stylesheet type="text/xsl" href="compatibility_result.xsl"?>
<Result start="1528285394762" end="1528286467560" start_display="Wed Jun 06 04:43:14 PDT 2018" end_display="Wed Jun 06 05:01:07 PDT 2018" suite_name="VTS" suite_version="9.0_R1" suite_plan="vts" suite_build_number="4000000" report_version="5.0" command_line_args="vts" devices="TEST123456" host_name="unit.test" os_name="Linux" os_version="4.13.0-41-generic" os_arch="amd64" java_vendor="Oracle Corporation" java_version="1.8.0_171">
  <Build build_abis_64="arm64-v8a" build_manufacturer="Google" build_reference_fingerprint2="" build_board="unit_test" build_serial="TEST123456" build_system_fingerprint="system_fp" build_reference_fingerprint="null" build_fingerprint="vendor_fp" build_abis="arm64-v8a,armeabi-v7a,armeabi" build_device="unit_test" build_abi="arm64-v8a" build_abi2="null" build_version_release="9" build_version_base_os="" build_type="userdebug" build_vendor_model="Unit Test" build_abis_32="armeabi-v7a,armeabi" build_product="unit_test" build_brand="google" build_abi22="" build_version_security_patch="2018-06-05" build_vendor_manufacturer="Google" build_version_sdk="28" build_id="test_build" build_model="Unit Test" build_vendor_fingerprint="vendor_fp" build_version_incremental="4000000" build_fingerprint2="system_fp" build_tags="test-keys" />
"""

_XML_1 = _XML_HEAD + """\
  <Summary pass="1" failed="3" modules_done="2" modules_total="2" />
  <Module name="module1" abi="armeabi-v7a" runtime="1" done="true" pass="0">
    <TestCase name="testcase1">
      <Test result="fail" name="test1" />
    </TestCase>
  </Module>
  <Module name="module2" abi="arm64-v8a" runtime="1" done="true" pass="1">
    <TestCase name="testcase2">
      <Test result="pass" name="test1" />
      <Test result="fail" name="test2" />
      <Test result="fail" name="test3" />
    </TestCase>
  </Module>
</Result>
"""

_XML_2 = _XML_HEAD + """\
  <Summary pass="3" failed="1" modules_done="2" modules_total="2" />
  <Module name="module1" abi="armeabi-v7a" runtime="1" done="true" pass="1">
    <TestCase name="testcase1">
      <Test result="pass" name="test1,test2" />
    </TestCase>
  </Module>
  <Module name="module2" abi="arm64-v8a" runtime="1" done="true" pass="2">
    <TestCase name="testcase2">
      <Test result="pass" name="test1" />
      <Test result="pass" name="test2" />
      <Test result="fail" name="test3" />
    </TestCase>
  </Module>
</Result>
"""

# Expected output
_CSV_HEAD = """\
logs,gs://a/b
logs,gs://c/vts
suite_name,VTS
suite_plan,vts
suite_version,9.0_R1
suite_build_number,4000000
start_display,Wed Jun 06 04:43:14 PDT 2018
end_display,Wed Jun 06 05:01:07 PDT 2018
build_fingerprint,vendor_fp
build_system_fingerprint,system_fp
build_vendor_fingerprint,vendor_fp
"""

_ALL_RESULTS_1 = _CSV_HEAD + """\
pass,1
failed,3
modules_total,2
modules_done,2
BITNESS,TEST_MODULE,TEST_CLASS,TEST_CASE,RESULT
32,module1,testcase1,test1,fail
64,module2,testcase2,test1,pass
64,module2,testcase2,test2,fail
64,module2,testcase2,test3,fail
"""

_FAILING_RESULTS_1 = _CSV_HEAD + """\
pass,1
failed,3
modules_total,2
modules_done,2
BITNESS,TEST_MODULE,TEST_CLASS,TEST_CASE,RESULT
32,module1,testcase1,test1,fail
64,module2,testcase2,test2,fail
64,module2,testcase2,test3,fail
"""

_TRUNCATED_RESULTS_1 = _CSV_HEAD + """\
pass,1
failed,3
modules_total,2
modules_done,2
BITNESS,TEST_MODULE,TEST_CLASS,TEST_CASE,RESULT
32,module1,testcase1,test1,fail
too many to be displayed
"""

_PRIMARY_ABI_RESULTS_1 = _CSV_HEAD + """\
pass,1
failed,3
modules_total,2
modules_done,2
BITNESS,TEST_MODULE,TEST_CLASS,TEST_CASE,RESULT
64,module2,testcase2,test1,pass
64,module2,testcase2,test2,fail
64,module2,testcase2,test3,fail
"""

_COMPARISON_1_2 = _CSV_HEAD + """\
pass,1
failed,3
modules_total,2
modules_done,2
BITNESS,TEST_MODULE,TEST_CLASS,TEST_CASE,RESULT,REFERENCE_RESULT
32,module1,testcase1,test1,fail,no_data
64,module2,testcase2,test2,fail,pass
64,module2,testcase2,test3,fail,fail
"""

_PRIMARY_ABI_COMPARISON_1_2 = _CSV_HEAD + """\
pass,1
failed,3
modules_total,2
modules_done,2
BITNESS,TEST_MODULE,TEST_CLASS,TEST_CASE,RESULT,REFERENCE_RESULT
64,module2,testcase2,test1,pass,
64,module2,testcase2,test2,fail,pass
64,module2,testcase2,test3,fail,fail
"""

_COMPARISON_2_1 = _CSV_HEAD + """\
pass,3
failed,1
modules_total,2
modules_done,2
BITNESS,TEST_MODULE,TEST_CLASS,TEST_CASE,RESULT,REFERENCE_RESULT
32,module1,testcase1,"test1,test2",pass,
64,module2,testcase2,test1,pass,
64,module2,testcase2,test2,pass,
64,module2,testcase2,test3,fail,fail
"""


@mock.patch("host_controller.command_processor.command_sheet.gspread")
@mock.patch("host_controller.command_processor.command_sheet."
            "ServiceAccountCredentials")
class CommandSheetTest(unittest.TestCase):
    """Unit test for CommandSheet.

    Attributes:
        _cmd: The CommandSheet being tested.
        _temp_files: The paths to the temporary files.
    """

    def setUp(self):
        """Creates CommandSheet."""
        self._cmd = command_sheet.CommandSheet()
        mock_console = mock.Mock()
        mock_console.FormatString = lambda x: x.replace("{suite_plan}", "vts")
        self._cmd._SetUp(mock_console)
        self._temp_files = []

    def tearDown(self):
        """Deletes CommandSheet and temoprary files."""
        self._cmd._TearDown()
        for temp_file in self._temp_files:
            os.remove(temp_file)

    def _CreateXml(self, xml_string):
        """Creates a test result in XML format.

        Args:
            xml_string: The file contents.

        Returns:
            The path to the XML file.
        """
        with tempfile.NamedTemporaryFile(
                suffix=".xml", delete=False) as temp_file:
            self._temp_files.append(temp_file.name)
            temp_file.write(xml_string)
            return temp_file.name

    def _CreateZip(self, xml_string):
        """Creates a zipped test result.

        Args:
            xml_string: The file contents.

        Returns:
            The path to the ZIP file.
        """
        with tempfile.NamedTemporaryFile(
                suffix=".zip", delete=False) as temp_file:
            self._temp_files.append(temp_file.name)
            with zipfile.ZipFile(temp_file, "w") as zip_file:
                zip_file.writestr(common._TEST_RESULT_XML, xml_string)
                return temp_file.name

    def testUploadZip(self, mock_credentials, mock_gspread):
        """Tests uploading zipped result."""
        mock_client = mock.Mock()
        mock_gspread.authorize.return_value = mock_client

        self._cmd.Run("--src %s --dest 123 --client_secret /abc "
                      "--extra_rows %s" %
                      (self._CreateZip(_XML_1), " ".join(_EXTRA_ROWS)))

        mock_client.import_csv.assert_called_with("123", _ALL_RESULTS_1)

    def testShowFailureOnly(self, mock_credentials, mock_gspread):
        """Tests showing only failing tests."""
        mock_client = mock.Mock()
        mock_gspread.authorize.return_value = mock_client

        self._cmd.Run("--src %s --dest 123 --client_secret /abc "
                      "--extra_rows %s --max 3" %
                      (self._CreateXml(_XML_1), " ".join(_EXTRA_ROWS)))

        mock_client.import_csv.assert_called_with("123", _FAILING_RESULTS_1)

    def testTruncate(self, mock_credentials, mock_gspread):
        """Tests truncating output."""
        mock_client = mock.Mock()
        mock_gspread.authorize.return_value = mock_client

        self._cmd.Run("--src %s --dest 123 --client_secret /abc "
                      "--extra_rows %s --max 1" %
                      (self._CreateXml(_XML_1), " ".join(_EXTRA_ROWS)))

        mock_client.import_csv.assert_called_with("123", _TRUNCATED_RESULTS_1)

    def testPrimaryAbiOnly(self, mock_credentials, mock_gspread):
        """Tests showing only results for primary ABI."""
        """Tests showing only failing tests."""
        mock_client = mock.Mock()
        mock_gspread.authorize.return_value = mock_client

        self._cmd.Run("--src %s --dest 123 --client_secret /abc "
                      "--extra_rows %s --primary_abi_only" %
                      (self._CreateXml(_XML_1), " ".join(_EXTRA_ROWS)))

        mock_client.import_csv.assert_called_with("123", _PRIMARY_ABI_RESULTS_1)

    def testCompareLocal(self, mock_credentials, mock_gspread):
        """Tests comparing two local XML files."""
        mock_client = mock.Mock()
        mock_gspread.authorize.return_value = mock_client

        self._cmd.Run("--src %s --ref %s --dest 123 --client_secret /abc "
                      "--extra_rows %s --max 3" %
                      (self._CreateXml(_XML_1), self._CreateZip(_XML_2),
                       " ".join(_EXTRA_ROWS)))

        mock_client.import_csv.assert_called_with("123", _COMPARISON_1_2)

    def testComparePrimaryAbi(self, mock_credentials, mock_gspread):
        """Tests comparing primary ABI only."""
        mock_client = mock.Mock()
        mock_gspread.authorize.return_value = mock_client

        self._cmd.Run("--src %s --ref %s --dest 123 --client_secret /abc "
                      "--extra_rows %s --primary_abi_only" %
                      (self._CreateXml(_XML_1), self._CreateZip(_XML_2),
                       " ".join(_EXTRA_ROWS)))

        mock_client.import_csv.assert_called_with("123",
                                                  _PRIMARY_ABI_COMPARISON_1_2)

    @mock.patch("host_controller.command_processor.command_sheet.gcs_utils")
    def testCompareGcs(self, mock_gcs_utils, mock_credentials, mock_gspread):
        """Tests comparing a local XML with a ZIP on GCS."""

        zip_name = "results_123.zip"
        gcs_dir = "gs://unit/test"
        zip_url = gcs_dir + "/" + zip_name

        def MockCopy(gsutil_path, gcs_url, local_dir):
            self.assertEqual(zip_url, gcs_url)
            with zipfile.ZipFile(os.path.join(local_dir, zip_name),
                                 "w") as zip_file:
                zip_file.writestr(common._TEST_RESULT_XML, _XML_1)
            return True

        mock_gcs_utils.GetGsutilPath.return_value = "mock_gsutil"
        mock_gcs_utils.IsGcsFile.return_value = False
        mock_gcs_utils.List.return_value = [zip_url]
        mock_gcs_utils.Copy = MockCopy

        mock_client = mock.Mock()
        mock_gspread.authorize.return_value = mock_client

        self._cmd.Run("--src %s --ref %s --dest 123 --client_secret /abc "
                      "--extra_rows %s" %
                      (self._CreateXml(_XML_2), gcs_dir,
                       " ".join(_EXTRA_ROWS)))

        mock_client.import_csv.assert_called_with("123", _COMPARISON_2_1)


if __name__ == "__main__":
    unittest.main()
