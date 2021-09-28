#!/usr/bin/env python

import copy
import re
import sys
import SELinuxNeverallowTestFrame

usage = "Usage: ./SELinuxNeverallowTestGen.py <input policy file> <output cts java source>"


class NeverallowRule:
    statement = ''
    depths = None

    def __init__(self, statement, depths):
        self.statement = statement
        self.depths = copy.deepcopy(depths)

# full-Treble only tests are inside sections delimited by BEGIN_{section} and
# END_{section} comments.
sections = ["TREBLE_ONLY", "COMPATIBLE_PROPERTY_ONLY", "LAUNCHING_WITH_R_ONLY"]

# extract_neverallow_rules - takes an intermediate policy file and pulls out the
# neverallow rules by taking all of the non-commented text between the 'neverallow'
# keyword and a terminating ';'
# returns: a list of rules
def extract_neverallow_rules(policy_file):
    with open(policy_file, 'r') as in_file:
        policy_str = in_file.read()

        # uncomment section delimiter lines
        section_headers = '|'.join(['BEGIN_%s|END_%s' % (s, s) for s in sections])
        remaining = re.sub(
            r'^\s*#\s*(' + section_headers + ')',
            r'\1',
            policy_str,
            flags = re.M)
        # remove comments
        remaining = re.sub(r'#.+?$', r'', remaining, flags = re.M)
        # match neverallow rules
        lines = re.findall(
            r'^\s*(neverallow\s.+?;|' + section_headers + ')',
            remaining,
            flags = re.M |re.S)

        # extract neverallow rules from the remaining lines
        rules = list()
        depths = dict()
        for section in sections:
            depths[section] = 0
        for line in lines:
            is_header = False
            for section in sections:
                if line.startswith("BEGIN_%s" % section):
                    depths[section] += 1
                    is_header = True
                    break
                elif line.startswith("END_%s" % section):
                    if depths[section] < 1:
                        exit("ERROR: END_%s outside of %s section" % (section, section))
                    depths[section] -= 1
                    is_header = True
                    break
            if not is_header:
                rule = NeverallowRule(line, depths)
                rules.append(rule)

        for section in sections:
            if depths[section] != 0:
                exit("ERROR: end of input while inside %s section" % section)

        return rules

# neverallow_rule_to_test - takes a neverallow statement and transforms it into
# the output necessary to form a cts unit test in a java source file.
# returns: a string representing a generic test method based on this rule.
def neverallow_rule_to_test(rule, test_num):
    squashed_neverallow = rule.statement.replace("\n", " ")
    method  = SELinuxNeverallowTestFrame.src_method
    method = method.replace("testNeverallowRules()",
        "testNeverallowRules" + str(test_num) + "()")
    method = method.replace("$NEVERALLOW_RULE_HERE$", squashed_neverallow)
    for section in sections:
        method = method.replace(
            "$%s_BOOL_HERE$" % section,
            "true" if rule.depths[section] else "false")
    return method

if __name__ == "__main__":
    # check usage
    if len(sys.argv) != 3:
        print usage
        exit(1)
    input_file = sys.argv[1]
    output_file = sys.argv[2]

    src_header = SELinuxNeverallowTestFrame.src_header
    src_body = SELinuxNeverallowTestFrame.src_body
    src_footer = SELinuxNeverallowTestFrame.src_footer

    # grab the neverallow rules from the policy file and transform into tests
    neverallow_rules = extract_neverallow_rules(input_file)
    i = 0
    for rule in neverallow_rules:
        src_body += neverallow_rule_to_test(rule, i)
        i += 1

    with open(output_file, 'w') as out_file:
        out_file.write(src_header)
        out_file.write(src_body)
        out_file.write(src_footer)
