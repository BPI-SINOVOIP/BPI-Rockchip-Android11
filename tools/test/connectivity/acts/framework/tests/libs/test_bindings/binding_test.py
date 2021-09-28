#!/usr/bin/env python3.4
#
#   Copyright 2017 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import unittest
import mock

from acts import signals

from acts.libs.test_binding.binding import Binding


class BindingTest(unittest.TestCase):

    def test_instance_binding(self):
        instance = object()
        binding = Binding(object)

        instance_binding = binding.__get__(instance, None)

        self.assertEqual(instance_binding.instance_args, [instance])

    def test_call_inner(self):
        self.inner_args = []
        self.inner_kwargs = {}

        def inner(*args, **kwargs):
            self.inner_args = args
            self.inner_kwargs = kwargs

        binding = Binding(inner, instance_args=['test'])

        try:
            binding('arg', v=2)
        except signals.TestSignal:
            pass

        self.assertEqual(self.inner_args, ('test', 'arg'))
        self.assertEqual(self.inner_kwargs, {'v': 2})

    def test_call_inner_pass_on_none(self):

        def inner(*args, **kwargs):
            pass

        binding = Binding(inner)

        try:
            binding()
        except signals.TestPass:
            pass

    def test_call_inner_pass_on_true(self):

        def inner(*args, **kwargs):
            return True

        binding = Binding(inner, instance_args=['test'])

        try:
            binding()
        except signals.TestPass:
            pass

    def test_call_inner_fail_on_false(self):

        def inner(*_, **__):
            return False

        binding = Binding(inner, instance_args=['test'])

        try:
            binding()
        except signals.TestFailure:
            pass

    def test_call_inner_pass_through_signal(self):

        def inner(*_, **__):
            raise signals.TestPass('DETAILS')

        binding = Binding(inner, instance_args=['test'])

        try:
            binding()
        except signals.TestPass as signal:
            self.assertEqual(signal.details, 'DETAILS')

    def test_arg_modifier(self):
        self.inner_args = []
        self.inner_kwargs = {}

        def arg_modifier(_, *args, **kwargs):
            new_args = list(args) + ['new arg']
            new_kwargs = dict(kwargs, kw='value')

            return new_args, new_kwargs

        def inner(*args, **kwargs):
            self.inner_args = args
            self.inner_kwargs = kwargs

        binding = Binding(inner, arg_modifier=arg_modifier)

        try:
            binding('arg', v=2)
        except signals.TestSignal:
            pass

        self.assertEqual(self.inner_args, ('arg', 'new arg'))
        self.assertEqual(self.inner_kwargs, {'v': 2, 'kw': 'value'})

    def test_call_before(self):

        self.has_called_before = False

        def before(*_, **__):
            self.has_called_before = True

        def inner(*_, **__):
            self.assertTrue(self.has_called_before)

        binding = Binding(inner, before=before)

        try:
            binding()
        except signals.TestSignal:
            pass

        self.assertTrue(self.has_called_before)

    def test_call_after(self):

        self.has_called_after = False

        def after(*_, **__):
            self.has_called_after = True

        def inner(*_, **__):
            self.assertFalse(self.has_called_after)

        binding = Binding(inner, after=after)

        try:
            binding()
        except signals.TestSignal:
            pass

        self.assertTrue(self.has_called_after)

    def test_signal_modify(self):

        def inner(*_, **__):
            raise signals.TestPass('DETAILS')

        def signal_modifier(_, signal, *__, **___):
            raise signals.TestFailure(signal.details)

        binding = Binding(inner, signal_modifier=signal_modifier)

        try:
            binding()
        except signals.TestFailure as signal:
            self.assertEqual(signal.details, 'DETAILS')

    def test_inner_attr_proxy_test(self):
        def some_func():
            pass

        inner = some_func
        inner.x = 10

        binding = Binding(inner)

        self.assertEqual(binding.x, inner.x)


if __name__ == "__main__":
    unittest.main()
