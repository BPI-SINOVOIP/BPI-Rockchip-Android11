#!/usr/bin/env python3

import re
import sys
import os.path
import subprocess
import xml.etree.cElementTree as ET

from collections import namedtuple

Subtest = namedtuple("Subtest", "name description")
KEYWORDS=re.compile(r'\b(invalid|hang|swap|thrash|crc|tiled|tiling|rte|ctx|render|blt|bsd|vebox|exec|rpm)\b')


def get_testlist(path):
    "read binaries' names from test-list.txt"
    with open(path, 'r') as f:
        assert(f.readline() == "TESTLIST\n")
        tests = f.readline().strip().split(" ")
        assert(f.readline() == "END TESTLIST\n")

    return tests


def keywordize(root, text, keywords):
    "set text for root element and wrap KEYWORDS in a <acronym>"
    matches = list(keywords.finditer(text))

    if not matches:
        root.text = text
        return

    pos = 0
    last_element = None
    root.text = ""

    for match in matches:
        if match.start() > pos:
            to_append = text[pos:match.start()]

            if last_element == None:
                root.text += to_append
            else:
                last_element.tail += to_append

        last_element = ET.SubElement(root, "acronym")
        last_element.tail = ""
        last_element.text=match.group()
        pos = match.end()

    last_element.tail = text[pos:]


def get_subtests(testdir, test):
    "execute test and get subtests with their descriptions via --describe"
    output = []
    full_test_path = os.path.join(testdir, test)
    proc = subprocess.run([full_test_path, "--describe"], stdout=subprocess.PIPE)
    description = ""
    current_subtest = None

    for line in proc.stdout.decode().splitlines():
        if line.startswith("SUB "):
            output += [Subtest(current_subtest, description)]
            description = ""
            current_subtest = line.split(' ')[1]
        else:
            description += line

    output += [Subtest(current_subtest, description)]

    return output

def main():
    output_file   = sys.argv[1]
    test_filter   = re.compile(sys.argv[2])
    testlist_file = sys.argv[3]
    testdir       = os.path.abspath(os.path.dirname(testlist_file))

    root = ET.Element("refsect1")
    ET.SubElement(root, "title").text = "Description"

    tests = get_testlist(testlist_file)

    for test in tests:
        if not test_filter.match(test):
            continue

        test_section = ET.SubElement(root, "refsect2", id=test)
        test_title = ET.SubElement(test_section, "title")
        keywordize(test_title, test, KEYWORDS)

        subtests = get_subtests(testdir, test)

        # we have description with no subtest name, add it at the top level
        if subtests and not subtests[0].name:
            ET.SubElement(test_section, "para").text = subtests[0].description

        if len(subtests) > 100:
            ET.SubElement(test_section, "para").text = "More than 100 subtests, skipping listing"
            continue

        for name, description in subtests:
            if not name:
                continue

            subtest_section = ET.SubElement(test_section, "refsect3", id="{}@{}".format(test, name))
            subtest_title = ET.SubElement(subtest_section, "title")
            keywordize(subtest_title, name, KEYWORDS)
            ET.SubElement(subtest_section, "para").text = description

    ET.ElementTree(root).write(output_file)

main()
