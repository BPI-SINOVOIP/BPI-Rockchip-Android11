#!/usr/bin/env python
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
import shutil
import tempfile

from vts.runners.host import asserts
from vts.runners.host import base_test
from vts.runners.host import const
from vts.runners.host import keys
from vts.runners.host import test_runner
from vts.utils.python.android import api
from vts.utils.python.file import target_file_utils


class VtsTrebleSysPropTest(base_test.BaseTestClass):
    """Test case which check compatibility of system property.

    Attributes:
        _temp_dir: The temporary directory to which necessary files are copied.
        _PUBLIC_PROPERTY_CONTEXTS_FILE_PATH:  The path of public property
                                              contexts file.
        _SYSTEM_PROPERTY_CONTEXTS_FILE_PATH:  The path of system property
                                              contexts file.
        _PRODUCT_PROPERTY_CONTEXTS_FILE_PATH: The path of product property
                                              contexts file.
        _VENDOR_PROPERTY_CONTEXTS_FILE_PATH:  The path of vendor property
                                              contexts file.
        _ODM_PROPERTY_CONTEXTS_FILE_PATH:     The path of odm property
                                              contexts file.
        _VENDOR_OR_ODM_NAMESPACES: The namespaces allowed for vendor/odm
                                   properties.
        _VENDOR_OR_ODM_NAMESPACES_WHITELIST: The extra namespaces allowed for
                                             vendor/odm properties.
        _VENDOR_TYPE_PREFIX: Expected prefix for the vendor prop types
        _ODM_TYPE_PREFIX: Expected prefix for the odm prop types
        _SYSTEM_WHITELISTED_TYPES: System props are not allowed to start with
            "vendor_", but these are exceptions.
        _VENDOR_OR_ODM_WHITELISTED_TYPES: vendor/odm props must start with
            "vendor_" or "odm_", but these are exceptions.
    """

    _PUBLIC_PROPERTY_CONTEXTS_FILE_PATH  = ("vts/testcases/security/"
                                            "system_property/data/"
                                            "property_contexts")
    _SYSTEM_PROPERTY_CONTEXTS_FILE_PATH  = ("/system/etc/selinux/"
                                            "plat_property_contexts")
    _PRODUCT_PROPERTY_CONTEXTS_FILE_PATH = ("/product/etc/selinux/"
                                            "product_property_contexts")
    _VENDOR_PROPERTY_CONTEXTS_FILE_PATH  = ("/vendor/etc/selinux/"
                                            "vendor_property_contexts")
    _ODM_PROPERTY_CONTEXTS_FILE_PATH     = ("/odm/etc/selinux/"
                                            "odm_property_contexts")
    _VENDOR_OR_ODM_NAMESPACES = [
            "ctl.odm.",
            "ctl.vendor.",
            "ctl.start$odm.",
            "ctl.start$vendor.",
            "ctl.stop$odm.",
            "ctl.stop$vendor.",
            "init.svc.odm.",
            "init.svc.vendor.",
            "ro.boot.",
            "ro.hardware.",
            "ro.odm.",
            "ro.vendor.",
            "odm.",
            "persist.odm.",
            "persist.vendor.",
            "vendor."
    ]

    _VENDOR_OR_ODM_NAMESPACES_WHITELIST = [
            "persist.camera." # b/138545066 remove this
    ]

    _VENDOR_TYPE_PREFIX = "vendor_"

    _ODM_TYPE_PREFIX = "odm_"

    _SYSTEM_WHITELISTED_TYPES = [
            "vendor_default_prop",
            "vendor_security_patch_level_prop",
            "vendor_socket_hook_prop"
    ]

    _VENDOR_OR_ODM_WHITELISTED_TYPES = [
    ]

    def setUpClass(self):
        """Initializes tests.

        Data file path, device, remote shell instance and temporary directory
        are initialized.
        """
        required_params = [keys.ConfigKeys.IKEY_DATA_FILE_PATH]
        self.getUserParams(required_params)
        self.dut = self.android_devices[0]
        self.shell = self.dut.shell
        self._temp_dir = tempfile.mkdtemp()

    def tearDownClass(self):
        """Deletes the temporary directory."""
        logging.info("Delete %s", self._temp_dir)
        shutil.rmtree(self._temp_dir)

    def _ParsePropertyDictFromPropertyContextsFile(self,
                                                   property_contexts_file,
                                                   exact_only=False):
        """Parse property contexts file to a dictionary.

        Args:
            property_contexts_file: file object of property contexts file
            exact_only: whether parsing only properties which require exact
                        matching

        Returns:
            dict: {property_name: property_tokens} where property_tokens[1]
            is selinux type of the property, e.g. u:object_r:my_prop:s0
        """
        property_dict = dict()
        for line in property_contexts_file.readlines():
            tokens = line.strip().rstrip("\n").split()
            if len(tokens) > 0 and not tokens[0].startswith("#"):
                if not exact_only:
                    property_dict[tokens[0]] = tokens
                elif len(tokens) >= 4 and tokens[2] == "exact":
                    property_dict[tokens[0]] = tokens

        return property_dict

    def testActionableCompatiblePropertyEnabled(self):
        """Ensures the feature of actionable compatible property is enforced.

        ro.actionable_compatible_property.enabled must be true to enforce the
        feature of actionable compatible property.
        """
        asserts.assertEqual(
            self.dut.getProp("ro.actionable_compatible_property.enabled"),
            "true", "ro.actionable_compatible_property.enabled must be true")

    def _TestVendorOrOdmPropertyNames(self, partition, contexts_path):
        logging.info("Checking existence of %s", contexts_path)
        target_file_utils.assertPermissionsAndExistence(
            self.shell, contexts_path, target_file_utils.IsReadable)

        # Pull property contexts file from device.
        self.dut.adb.pull(contexts_path, self._temp_dir)
        logging.info("Adb pull %s to %s", contexts_path, self._temp_dir)

        with open(
                os.path.join(self._temp_dir,
                             "%s_property_contexts" % partition),
                "r") as property_contexts_file:
            property_dict = self._ParsePropertyDictFromPropertyContextsFile(
                property_contexts_file)
        logging.info("Found %d property names in %s property contexts",
                     len(property_dict), partition)
        violation_list = filter(
            lambda x: not any(
                x.startswith(prefix) for prefix in
                self._VENDOR_OR_ODM_NAMESPACES + self._VENDOR_OR_ODM_NAMESPACES_WHITELIST),
            property_dict.keys())
        asserts.assertEqual(
            len(violation_list), 0,
            ("%s properties (%s) have wrong namespace" %
             (partition, " ".join(sorted(violation_list)))))

    def _TestPropertyTypes(self, property_contexts_file, check_function):
        fd, downloaded = tempfile.mkstemp(dir=self._temp_dir)
        os.close(fd)
        self.dut.adb.pull(property_contexts_file, downloaded)
        logging.info("adb pull %s to %s", property_contexts_file, downloaded)

        with open(downloaded, "r") as f:
            property_dict = self._ParsePropertyDictFromPropertyContextsFile(f)
        logging.info("Found %d properties from %s",
                     len(property_dict), property_contexts_file)

        # Filter props that don't satisfy check_function.
        # tokens[1] is something like u:object_r:my_prop:s0
        violation_list = [(name, tokens) for name, tokens in
                          property_dict.items()
                          if not check_function(tokens[1].split(":")[2])]

        asserts.assertEqual(
            len(violation_list), 0,
            "properties in %s have wrong property types:\n%s" % (
                property_contexts_file,
                "\n".join("name: %s, type: %s" % (name, tokens[1])
                          for name, tokens in violation_list))
        )

    def testVendorPropertyNames(self):
        """Ensures vendor properties have proper namespace.

        Vendor or ODM properties must have their own prefix.
        """
        asserts.skipIf(
            self.dut.getLaunchApiLevel() <= api.PLATFORM_API_LEVEL_P,
            "Skip test for a device which launched first before Android Q.")

        self._TestVendorOrOdmPropertyNames(
            "vendor", self._VENDOR_PROPERTY_CONTEXTS_FILE_PATH)

    def testOdmPropertyNames(self):
        """Ensures odm properties have proper namespace.

        Vendor or ODM properties must have their own prefix.
        """
        asserts.skipIf(
            self.dut.getLaunchApiLevel() <= api.PLATFORM_API_LEVEL_P,
            "Skip test for a device which launched first before Android Q.")

        asserts.skipIf(
            not target_file_utils.Exists(self._ODM_PROPERTY_CONTEXTS_FILE_PATH,
                                         self.shell),
            "Skip test for a device which doesn't have an odm property "
            "contexts.")

        self._TestVendorOrOdmPropertyNames(
            "odm", self._ODM_PROPERTY_CONTEXTS_FILE_PATH)

    def testProductPropertyNames(self):
        """Ensures product properties have proper namespace.

        Product properties must not have Vendor or ODM namespaces.
        """
        asserts.skipIf(
            self.dut.getLaunchApiLevel() <= api.PLATFORM_API_LEVEL_P,
            "Skip test for a device which launched first before Android Q.")

        asserts.skipIf(
            not target_file_utils.Exists(self._PRODUCT_PROPERTY_CONTEXTS_FILE_PATH,
                                         self.shell),
            "Skip test for a device which doesn't have an product property "
            "contexts.")
        logging.info("Checking existence of %s",
                     self._PRODUCT_PROPERTY_CONTEXTS_FILE_PATH)
        target_file_utils.assertPermissionsAndExistence(
            self.shell, self._PRODUCT_PROPERTY_CONTEXTS_FILE_PATH,
            target_file_utils.IsReadable)

        # Pull product property contexts file from device.
        self.dut.adb.pull(self._PRODUCT_PROPERTY_CONTEXTS_FILE_PATH,
                          self._temp_dir)
        logging.info("Adb pull %s to %s",
                     self._PRODUCT_PROPERTY_CONTEXTS_FILE_PATH, self._temp_dir)

        with open(os.path.join(self._temp_dir, "product_property_contexts"),
                  "r") as property_contexts_file:
            property_dict = self._ParsePropertyDictFromPropertyContextsFile(
                property_contexts_file, True)
        logging.info(
            "Found %d property names in product property contexts",
            len(property_dict))

        violation_list = filter(
            lambda x: any(
                x.startswith(prefix)
                for prefix in self._VENDOR_OR_ODM_NAMESPACES),
            property_dict.keys())
        asserts.assertEqual(
            len(violation_list), 0,
            ("product propertes (%s) have wrong namespace" %
             " ".join(sorted(violation_list))))

    def testPlatformPropertyTypes(self):
        """Ensures properties in the system partition have valid types"""

        asserts.skipIf(
            self.dut.getLaunchApiLevel() <= api.PLATFORM_API_LEVEL_Q,
            "Skip test for a device which launched first before Android R.")

        self._TestPropertyTypes(
            self._SYSTEM_PROPERTY_CONTEXTS_FILE_PATH,
            lambda typename: (
                not typename.startswith(self._VENDOR_TYPE_PREFIX) and
                not typename.startswith(self._ODM_TYPE_PREFIX) and
                typename not in self._VENDOR_OR_ODM_WHITELISTED_TYPES
            ) or typename in self._SYSTEM_WHITELISTED_TYPES)

    def testVendorPropertyTypes(self):
        """Ensures properties in the vendor partion have valid types"""

        asserts.skipIf(
            self.dut.getLaunchApiLevel() <= api.PLATFORM_API_LEVEL_Q,
            "Skip test for a device which launched first before Android R.")

        self._TestPropertyTypes(
            self._VENDOR_PROPERTY_CONTEXTS_FILE_PATH,
            lambda typename: typename.startswith(self._VENDOR_TYPE_PREFIX) or
            typename in self._VENDOR_OR_ODM_WHITELISTED_TYPES)

    def testOdmPropertyTypes(self):
        """Ensures properties in the odm partition have valid types"""

        asserts.skipIf(
            self.dut.getLaunchApiLevel() <= api.PLATFORM_API_LEVEL_Q,
            "Skip test for a device which launched first before Android R.")

        asserts.skipIf(
            not target_file_utils.Exists(self._ODM_PROPERTY_CONTEXTS_FILE_PATH,
                                         self.shell),
            "Skip test for a device which doesn't have an odm property "
            "contexts.")

        self._TestPropertyTypes(
            self._ODM_PROPERTY_CONTEXTS_FILE_PATH,
            lambda typename: typename.startswith(self._VENDOR_TYPE_PREFIX) or
            typename.startswith(self._ODM_TYPEPREFIX) or
            typename in self._VENDOR_OR_ODM_WHITELISTED_TYPES)

    def testExportedPlatformPropertyIntegrity(self):
        """Ensures public property contexts isn't modified at all.

        Public property contexts must not be modified.
        """
        logging.info("Checking existence of %s",
                     self._SYSTEM_PROPERTY_CONTEXTS_FILE_PATH)
        target_file_utils.assertPermissionsAndExistence(
            self.shell, self._SYSTEM_PROPERTY_CONTEXTS_FILE_PATH,
            target_file_utils.IsReadable)

        # Pull system property contexts file from device.
        self.dut.adb.pull(self._SYSTEM_PROPERTY_CONTEXTS_FILE_PATH,
                          self._temp_dir)
        logging.info("Adb pull %s to %s",
                     self._SYSTEM_PROPERTY_CONTEXTS_FILE_PATH, self._temp_dir)

        with open(os.path.join(self._temp_dir, "plat_property_contexts"),
                  "r") as property_contexts_file:
            sys_property_dict = self._ParsePropertyDictFromPropertyContextsFile(
                property_contexts_file, True)
        logging.info(
            "Found %d exact-matching properties "
            "in system property contexts", len(sys_property_dict))

        pub_property_contexts_file_path = os.path.join(
            self.data_file_path, self._PUBLIC_PROPERTY_CONTEXTS_FILE_PATH)
        with open(pub_property_contexts_file_path,
                  "r") as property_contexts_file:
            pub_property_dict = self._ParsePropertyDictFromPropertyContextsFile(
                property_contexts_file, True)

        for name in pub_property_dict:
            public_tokens = pub_property_dict[name]
            asserts.assertTrue(name in sys_property_dict,
                               "Exported property (%s) doesn't exist" % name)
            system_tokens = sys_property_dict[name]
            asserts.assertEqual(public_tokens, system_tokens,
                                "Exported property (%s) is modified" % name)


if __name__ == "__main__":
    test_runner.main()
