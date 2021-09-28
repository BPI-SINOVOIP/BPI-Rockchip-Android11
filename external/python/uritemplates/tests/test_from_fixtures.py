import io
import json
import os.path

import uritemplate


def fixture_file_path(filename):
    absolute_dir = os.path.abspath(os.path.dirname(__file__))
    filename = filename + '.json'
    return os.path.join(absolute_dir, 'fixtures', filename)


def load_examples(filename):
    path = fixture_file_path(filename)
    with io.open(path, 'r', encoding="utf-8") as examples_file:
        examples = json.load(examples_file)
    return examples


def expected_set(expected):
    if isinstance(expected, list):
        return set(expected)
    return {expected}


class FixtureMixin(object):
    def _get_test(self, section):
        test = self.examples.get(section, {})
        return test.get('variables', {}), test.get('testcases', [])

    def _test(self, testname):
        variables, testcases = self._get_test(testname)
        for template, expected in testcases:
            expected = expected_set(expected)
            expanded = uritemplate.expand(template, variables)
            assert expanded in expected


class TestSpecExamples(FixtureMixin):
    examples = load_examples('spec-examples')

    def test_level_1(self):
        """Check that uritemplate.expand matches Level 1 expectations."""
        self._test('Level 1 Examples')

    def test_level_2(self):
        """Check that uritemplate.expand matches Level 2 expectations."""
        self._test('Level 2 Examples')

    def test_level_3(self):
        """Check that uritemplate.expand matches Level 3 expectations."""
        self._test('Level 3 Examples')

    def test_level_4(self):
        """Check that uritemplate.expand matches Level 4 expectations."""
        self._test('Level 4 Examples')


class TestSpecExamplesByRFCSection(FixtureMixin):
    examples = load_examples('spec-examples-by-section')

    def test_variable_expansion(self):
        """Check variable expansion."""
        self._test('3.2.1 Variable Expansion')

    def test_simple_string_expansion(self):
        """Check simple string expansion."""
        self._test('3.2.2 Simple String Expansion')

    def test_reserved_expansion(self):
        """Check reserved expansion."""
        self._test('3.2.3 Reserved Expansion')

    def test_fragment_expansion(self):
        """Check fragment expansion."""
        self._test('3.2.4 Fragment Expansion')

    def test_dot_prefixed_label_expansion(self):
        """Check label expansion with dot-prefix."""
        self._test('3.2.5 Label Expansion with Dot-Prefix')

    def test_path_segment_expansion(self):
        """Check path segment expansion."""
        self._test('3.2.6 Path Segment Expansion')

    def test_path_style_parameter_expansion(self):
        """Check path-style param expansion."""
        self._test('3.2.7 Path-Style Parameter Expansion')

    def test_form_style_query_expansion(self):
        """Check form-style query expansion."""
        self._test('3.2.8 Form-Style Query Expansion')

    def test_form_style_query_cntinuation(self):
        """Check form-style query continuation."""
        self._test('3.2.9 Form-Style Query Continuation')


class TestExtendedTests(FixtureMixin):
    examples = load_examples('extended-tests')

    def test_additional_examples_1(self):
        """Check Additional Examples 1."""
        self._test('Additional Examples 1')

    def test_additional_examples_2(self):
        """Check Additional Examples 2."""
        self._test('Additional Examples 2')

    def test_additional_examples_3(self):
        """Check Additional Examples 3."""
        self._test('Additional Examples 3: Empty Variables')

    def test_additional_examples_4(self):
        """Check Additional Examples 4."""
        self._test('Additional Examples 4: Numeric Keys')
