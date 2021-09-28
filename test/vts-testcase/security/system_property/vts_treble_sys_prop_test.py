#!/usr/bin/env python3
#
# Copyright (C) 2020 The Android Open Source Project
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
import unittest

from importlib import resources

from vts.testcases.vndk import utils
from vts.utils.python.android import api

PERMISSION_GROUPS = 3  # 3 permission groups: owner, group, all users
READ_PERMISSION = 4
WRITE_PERMISSION = 2
EXECUTE_PERMISSION = 1

def HasPermission(permission_bits, groupIndex, permission):
    """Determines if the permission bits grant a permission to a group.

    Args:
        permission_bits: string, the octal permissions string (e.g. 741)
        groupIndex: int, the index of the group into the permissions string.
                    (e.g. 0 is owner group). If set to -1, then all groups are
                    checked.
        permission: the value of the permission.

    Returns:
        True if the group(s) has read permission.

    Raises:
        ValueError if the group or permission bits are invalid
    """
    if groupIndex >= PERMISSION_GROUPS:
        raise ValueError("Invalid group: %s" % str(groupIndex))

    if len(permission_bits) != PERMISSION_GROUPS:
        raise ValueError("Invalid permission bits: %s" % str(permission_bits))

    # Define the start/end group index
    start = groupIndex
    end = groupIndex + 1
    if groupIndex < 0:
        start = 0
        end = PERMISSION_GROUPS

    for i in range(start, end):
        perm = int(permission_bits[i])  # throws ValueError if not an integer
        if perm > 7:
            raise ValueError("Invalid permission bit: %s" % str(perm))
        if perm & permission == 0:
            # Return false if any group lacks the permission
            return False
    # Return true if no group lacks the permission
    return True


def IsReadable(permission_bits):
    """Determines if the permission bits grant read permission to any group.

    Args:
        permission_bits: string, the octal permissions string (e.g. 741)

    Returns:
        True if any group has read permission.

    Raises:
        ValueError if the group or permission bits are invalid
    """
    return any([
        HasPermission(permission_bits, i, READ_PERMISSION)
        for i in range(PERMISSION_GROUPS)
    ])

class VtsTrebleSysPropTest(unittest.TestCase):
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

    _PUBLIC_PROPERTY_CONTEXTS_FILE_PATH  = ("public/property_contexts")
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

    def setUp(self):
        """Initializes tests.

        Data file path, device, remote shell instance and temporary directory
        are initialized.
        """
        serial_number = os.environ.get("ANDROID_SERIAL")
        self.assertTrue(serial_number, "$ANDROID_SERIAL is empty.")
        self.dut = utils.AndroidDevice(serial_number)
        self._temp_dir = tempfile.mkdtemp()

    def tearDown(self):
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
        self.assertEqual(
            self.dut._GetProp("ro.actionable_compatible_property.enabled"),
            "true", "ro.actionable_compatible_property.enabled must be true")

    def _TestVendorOrOdmPropertyNames(self, partition, contexts_path):
        logging.info("Checking existence of %s", contexts_path)
        self.AssertPermissionsAndExistence(
            contexts_path, IsReadable)

        # Pull property contexts file from device.
        self.dut.AdbPull(contexts_path, self._temp_dir)
        logging.info("Adb pull %s to %s", contexts_path, self._temp_dir)

        with open(
                os.path.join(self._temp_dir,
                             "%s_property_contexts" % partition),
                "r") as property_contexts_file:
            property_dict = self._ParsePropertyDictFromPropertyContextsFile(
                property_contexts_file)
        logging.info("Found %d property names in %s property contexts",
                     len(property_dict), partition)
        violation_list = list(filter(
            lambda x: not any(
                x.startswith(prefix) for prefix in
                self._VENDOR_OR_ODM_NAMESPACES +
                self._VENDOR_OR_ODM_NAMESPACES_WHITELIST),
            property_dict.keys()))
        self.assertEqual(
            # Transfer filter to list for python3.
            len(violation_list), 0,
            ("%s properties (%s) have wrong namespace" %
             (partition, " ".join(sorted(violation_list)))))

    def _TestPropertyTypes(self, property_contexts_file, check_function):
        fd, downloaded = tempfile.mkstemp(dir=self._temp_dir)
        os.close(fd)
        self.dut.AdbPull(property_contexts_file, downloaded)
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

        self.assertEqual(
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
        if self.dut.GetLaunchApiLevel() <= api.PLATFORM_API_LEVEL_P:
            logging.info("Skip test for a device which launched first before "
                         "Android Q.")
            return
        self._TestVendorOrOdmPropertyNames(
            "vendor", self._VENDOR_PROPERTY_CONTEXTS_FILE_PATH)


    def testOdmPropertyNames(self):
        """Ensures odm properties have proper namespace.

        Vendor or ODM properties must have their own prefix.
        """
        if self.dut.GetLaunchApiLevel() <= api.PLATFORM_API_LEVEL_P:
            logging.info("Skip test for a device which launched first before "
                         "Android Q.")
            return
        if (not self.dut.Exists(self._ODM_PROPERTY_CONTEXTS_FILE_PATH)):
            logging.info("Skip test for a device which doesn't have an odm "
                         "property contexts.")
            return
        self._TestVendorOrOdmPropertyNames(
            "odm", self._ODM_PROPERTY_CONTEXTS_FILE_PATH)

    def testProductPropertyNames(self):
        """Ensures product properties have proper namespace.

        Product properties must not have Vendor or ODM namespaces.
        """
        if self.dut.GetLaunchApiLevel() <= api.PLATFORM_API_LEVEL_P:
            logging.info("Skip test for a device which launched first before "
                         "Android Q.")
            return
        if (not self.dut.Exists(self._PRODUCT_PROPERTY_CONTEXTS_FILE_PATH)):
            logging.info("Skip test for a device which doesn't have an product "
                         "property contexts.")
            return

        logging.info("Checking existence of %s",
                     self._PRODUCT_PROPERTY_CONTEXTS_FILE_PATH)
        self.AssertPermissionsAndExistence(
            self._PRODUCT_PROPERTY_CONTEXTS_FILE_PATH,
            IsReadable)

        # Pull product property contexts file from device.
        self.dut.AdbPull(self._PRODUCT_PROPERTY_CONTEXTS_FILE_PATH,
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

        violation_list = list(filter(
            lambda x: any(
                x.startswith(prefix)
                for prefix in self._VENDOR_OR_ODM_NAMESPACES),
            property_dict.keys()))
        self.assertEqual(
            len(violation_list), 0,
            ("product propertes (%s) have wrong namespace" %
             " ".join(sorted(violation_list))))

    def testPlatformPropertyTypes(self):
        """Ensures properties in the system partition have valid types"""
        if self.dut.GetLaunchApiLevel() <= api.PLATFORM_API_LEVEL_Q:
            logging.info("Skip test for a device which launched first before "
                         "Android Q.")
            return
        self._TestPropertyTypes(
            self._SYSTEM_PROPERTY_CONTEXTS_FILE_PATH,
            lambda typename: (
                not typename.startswith(self._VENDOR_TYPE_PREFIX) and
                not typename.startswith(self._ODM_TYPE_PREFIX) and
                typename not in self._VENDOR_OR_ODM_WHITELISTED_TYPES
            ) or typename in self._SYSTEM_WHITELISTED_TYPES)

    def testVendorPropertyTypes(self):
        """Ensures properties in the vendor partion have valid types"""
        if self.dut.GetLaunchApiLevel() <= api.PLATFORM_API_LEVEL_Q:
            logging.info("Skip test for a device which launched first before "
                         "Android Q.")
            return
        self._TestPropertyTypes(
            self._VENDOR_PROPERTY_CONTEXTS_FILE_PATH,
            lambda typename: typename.startswith(self._VENDOR_TYPE_PREFIX) or
            typename in self._VENDOR_OR_ODM_WHITELISTED_TYPES)

    def testOdmPropertyTypes(self):
        """Ensures properties in the odm partition have valid types"""
        if self.dut.GetLaunchApiLevel() <= api.PLATFORM_API_LEVEL_Q:
            logging.info("Skip test for a device which launched first before "
                         "Android Q.")
            return
        if (not self.dut.Exists(self._ODM_PROPERTY_CONTEXTS_FILE_PATH)):
            logging.info("Skip test for a device which doesn't have an odm "
                         "property contexts.")
            return
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
        self.AssertPermissionsAndExistence(
            self._SYSTEM_PROPERTY_CONTEXTS_FILE_PATH,
            IsReadable)

        # Pull system property contexts file from device.
        self.dut.AdbPull(self._SYSTEM_PROPERTY_CONTEXTS_FILE_PATH,
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

        # Extract data from parfile.
        resource_name = os.path.basename(self._PUBLIC_PROPERTY_CONTEXTS_FILE_PATH)
        package_name = os.path.dirname(
            self._PUBLIC_PROPERTY_CONTEXTS_FILE_PATH).replace(os.path.sep, '.')
        with resources.open_text(package_name, resource_name) as resource:
            pub_property_dict = self._ParsePropertyDictFromPropertyContextsFile(
                resource, True)
        for name in pub_property_dict:
            public_tokens = pub_property_dict[name]
            self.assertTrue(name in sys_property_dict,
                               "Exported property (%s) doesn't exist" % name)
            system_tokens = sys_property_dict[name]
            self.assertEqual(public_tokens, system_tokens,
                                "Exported property (%s) is modified" % name)


    def AssertPermissionsAndExistence(self, path, check_permission):
        """Asserts that the specified path exists and has the correct permission.
        Args:
            path: string, path to validate existence and permissions
            check_permission: function which takes unix permissions in octalformat
                              and returns True if the permissions are correct,
                              False otherwise.
        """
        self.assertTrue(self.dut.Exists(path), "%s: File does not exist." % path)
        try:
            permission = self.GetPermission(path)
            self.assertTrue(check_permission(permission),
                            "%s: File has invalid permissions (%s)" % (path, permission))
        except (ValueError, IOError) as e:
            assertIsNone(e, "Failed to assert permissions: %s" % str(e))

    def GetPermission(self, path):
        """Read the file permission bits of a path.

        Args:
            filepath: string, path to a file or directory

        Returns:
            String, octal permission bits for the path

        Raises:
            IOError if the path does not exist or has invalid permission bits.
        """
        cmd = "stat -c %%a %s" % path
        out, err, return_code =  self.dut.Execute(cmd)
        logging.debug("%s: Shell command '%s' out: %s, err: %s, return_code: %s", path, cmd, out, err, return_code)
        # checks the exit code
        if return_code != 0:
            raise IOError(err)
        accessBits = out.strip()
        if len(accessBits) != 3:
            raise IOError("%s: Wrong number of access bits (%s)" % (path, accessBits))
        return accessBits

if __name__ == "__main__":
    # Setting verbosity is required to generate output that the TradeFed test
    # runner can parse.
    unittest.main(verbosity=3)
