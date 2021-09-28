#
# Copyright (C) 2018 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import logging
import os

from xml.etree import ElementTree

from host_controller import common


def ExtractResultZip(zip_file, output_dir, xml_only=True):
    """Extracts XML result from a zip file.

    Args:
        zip_file: An object of zipfile.ZipFile.
        output_dir: The directory where the zip is extracted.
        xml_only: Whether to extract log-result.xml or test_result.xml only.

    Returns:
        The path to the XML in the output directory.
        None if the zip does not contain the XML result.
    """
    try:
        xml_name = next(x for x in zip_file.namelist() if
                        x.endswith(common._TEST_RESULT_XML) or
                        x.endswith(common._LOG_RESULT_XML))
    except StopIteration:
        return None

    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    if xml_only:
        return zip_file.extract(xml_name, path=output_dir)
    else:
        zip_file.extractall(path=output_dir)
        return os.path.join(output_dir, xml_name)


def LoadTestSummary(result_xml):
    """Gets attributes of <Result>, <Build>, and <Summary>.

    Args:
        result_xml: A file object of the TradeFed report in XML format.

    Returns:
        3 dictionaries, the attributes of <Result>, <Build>, and <Summary>.
    """
    result_attrib = {}
    build_attrib = {}
    summary_attrib = {}
    for event, elem in ElementTree.iterparse(result_xml, events=("start", )):
        if all((result_attrib, build_attrib, summary_attrib)):
            break
        if elem.tag == common._RESULT_TAG:
            result_attrib = dict(elem.attrib)
        elif elem.tag == common._BUILD_TAG:
            build_attrib = dict(elem.attrib)
        elif elem.tag == common._SUMMARY_TAG:
            summary_attrib = dict(elem.attrib)
    return result_attrib, build_attrib, summary_attrib


def IterateTestResults(result_xml):
    """Yields test records in test_result.xml.

    Args:
        result_xml: A file object of the TradeFed report in XML format.

    Yields:
        A tuple of ElementTree.Element, (Module, TestCase, Test) for each
        </Test>.
    """
    module_elem = None
    testcase_elem = None
    for event, elem in ElementTree.iterparse(
            result_xml, events=("start", "end")):
        if event == "start":
            if elem.tag == common._MODULE_TAG:
                module_elem = elem
            elif elem.tag == common._TESTCASE_TAG:
                testcase_elem = elem

        if event == "end" and elem.tag == common._TEST_TAG:
            yield module_elem, testcase_elem, elem


def GetAbiBitness(abi):
    """Gets bitness of an ABI.

    Args:
        abi: A string, the ABI name.

    Returns:
        32 or 64, the ABI bitness.
    """
    return 64 if "arm64" in abi or "x86_64" in abi else 32


def GetTestName(module, testcase, test):
    """Gets the bitness and the full test name.

    Args:
        module: The Element for <Module>.
        testcase: The Element for <TestCase>.
        test: The Element for <Test>.

    Returns:
        A tuple of (bitness, module_name, testcase_name, test_name).
    """
    abi = module.attrib.get(common._ABI_ATTR_KEY, "")
    bitness = str(GetAbiBitness(abi))
    module_name = module.attrib.get(common._NAME_ATTR_KEY, "")
    testcase_name = testcase.attrib.get(common._NAME_ATTR_KEY, "")
    test_name = test.attrib.get(common._NAME_ATTR_KEY, "")
    return bitness, module_name, testcase_name, test_name
