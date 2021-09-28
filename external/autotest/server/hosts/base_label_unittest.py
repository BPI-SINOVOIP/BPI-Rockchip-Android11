#!/usr/bin/python2
# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import common

from autotest_lib.server import utils
from autotest_lib.server.hosts import host_info
from autotest_lib.server.hosts import base_label

 # pylint: disable=missing-docstring


class TestBaseLabel(base_label.BaseLabel):
    """TestBaseLabel is used for testing/validating BaseLabel methods."""

    _NAME = 'base_label'

    def exists(self, host):
        return host.exists


class TestBaseLabels(base_label.BaseLabel):
    """
    TestBaseLabels is used for testing/validating BaseLabel methods.

    This is a variation of BaseLabel with multiple labels for _NAME
    to ensure we handle a label that contains a list of labels for
    its _NAME attribute.
    """

    _NAME = ['base_label_1' , 'base_label_2']


class TestStringPrefixLabel(base_label.StringPrefixLabel):
    """
    TestBaseLabels is used for testing/validating StringPrefixLabel methods.

    This test class is to check that we properly construct the prefix labels
    with the label passed in during construction.
    """

    _NAME = 'prefix'

    def __init__(self, label='postfix'):
        self.label_to_return = label


    def generate_labels(self, _):
        return [self.label_to_return]


class MockAFEHost(utils.EmptyAFEHost):

    def __init__(self, labels=None, attributes=None):
        self.labels = labels or []
        self.attributes = attributes or {}


class MockHost(object):

    def __init__(self, exists=True, store=None):
        self.hostname = 'hostname'
        self.exists = exists
        self.host_info_store = store


class BaseLabelUnittests(unittest.TestCase):
    """Unittest for testing base_label.BaseLabel."""

    def setUp(self):
        self.test_base_label = TestBaseLabel()
        self.test_base_labels = TestBaseLabels()


    def test_generate_labels(self):
        """Let's make sure generate_labels() returns the labels expected."""
        self.assertEqual(self.test_base_label.generate_labels(None),
                         [self.test_base_label._NAME])


    def test_get(self):
        """Let's make sure the logic in get() works as expected."""
        # We should get labels here.
        self.assertEqual(self.test_base_label.get(MockHost(exists=True)),
                         [self.test_base_label._NAME])
        # We should get nothing here.
        self.assertEqual(self.test_base_label.get(MockHost(exists=False)),
                         [])


    def test_get_all_labels(self):
        """Check that we get the expected labels for get_all_labels()."""
        prefix_tbl, full_tbl = self.test_base_label.get_all_labels()
        prefix_tbls, full_tbls = self.test_base_labels.get_all_labels()

        # We want to check that we always get a list of labels regardless if
        # the label class attribute _NAME is a list or a string.
        self.assertEqual(full_tbl, set([self.test_base_label._NAME]))
        self.assertEqual(full_tbls, set(self.test_base_labels._NAME))

        # We want to make sure we get nothing on the prefix_* side of things
        # since BaseLabel shouldn't be a prefix for any label.
        self.assertEqual(prefix_tbl, set())
        self.assertEqual(prefix_tbls, set())

    def test_update_for_task(self):
        self.assertTrue(self.test_base_label.update_for_task(''))


class StringPrefixLabelUnittests(unittest.TestCase):
    """Unittest for testing base_label.StringPrefixLabel."""

    def setUp(self):
        self.postfix_label = 'postfix_label'
        self.test_label = TestStringPrefixLabel(label=self.postfix_label)


    def test_get(self):
        """Let's make sure that the labels we get are prefixed."""
        self.assertEqual(self.test_label.get(None),
                         ['%s:%s' % (self.test_label._NAME,
                                     self.postfix_label)])


    def test_get_all_labels(self):
        """Check that we only get prefix labels and no full labels."""
        prefix_labels, postfix_labels = self.test_label.get_all_labels()
        self.assertEqual(prefix_labels, set(['%s:' % self.test_label._NAME]))
        self.assertEqual(postfix_labels, set())


class LabelRetrieverUnittests(unittest.TestCase):
    """Unittest for testing base_label.LabelRetriever."""

    def setUp(self):
        label_list = [TestStringPrefixLabel(), TestBaseLabel()]
        self.retriever = base_label.LabelRetriever(label_list)
        self.retriever._populate_known_labels(label_list, '')


    def test_populate_known_labels(self):
        """Check that _populate_known_labels() works as expected."""
        full_names = set([TestBaseLabel._NAME])
        prefix_names = set(['%s:' % TestStringPrefixLabel._NAME])
        # Check on a normal retriever.
        self.assertEqual(self.retriever.label_full_names, full_names)
        self.assertEqual(self.retriever.label_prefix_names, prefix_names)


    def test_is_known_label(self):
        """Check _is_known_label() detects/skips the right labels."""
        # This will be a list of tuples of label and expected return bool.
        # Make sure Full matches match correctly
        labels_to_check = [(TestBaseLabel._NAME, True),
                           ('%s:' % TestStringPrefixLabel._NAME, True),
                           # Make sure partial matches fail.
                           (TestBaseLabel._NAME[:2], False),
                           ('%s:' % TestStringPrefixLabel._NAME[:2], False),
                           ('no_label_match', False)]

        for label, expected_known in labels_to_check:
            self.assertEqual(self.retriever._is_known_label(label),
                             expected_known)


    def test_update_labels(self):
        """Check that we add/remove the expected labels in update_labels()."""
        label_to_add = 'label_to_add'
        label_to_remove = 'prefix:label_to_remove'
        store = host_info.InMemoryHostInfoStore(
                info=host_info.HostInfo(
                        labels=[label_to_remove, TestBaseLabel._NAME],
                ),
        )
        mockhost = MockHost(store=store)

        retriever = base_label.LabelRetriever(
                [TestStringPrefixLabel(label=label_to_add),
                 TestBaseLabel()])
        retriever.update_labels(mockhost)
        self.assertEqual(
                set(store.get().labels),
                {'%s:%s' % (TestStringPrefixLabel._NAME, label_to_add),
                 TestBaseLabel._NAME},
        )


if __name__ == '__main__':
    unittest.main()

