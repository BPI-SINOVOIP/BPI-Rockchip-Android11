# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import tempfile

from autotest_lib.client.common_lib import error
from autotest_lib.server.cros.faft.firmware_test import FirmwareTest


class platform_FlashErasers(FirmwareTest):
    """
    Test that various erase functions work correctly by calling flashrom to
    erase/write blocks of different sizes.
    """
    version = 1

    def run_cmd(self, command, checkfor=''):
        """
        Log and execute command and return the output.

        @param command: Command to execute on device.
        @param checkfor: If not emmpty, fail test if checkfor not in output.
        @returns the output of command as a list of strings.
        """
        command = command + ' 2>&1'
        logging.info('Execute %s', command)
        output = self.faft_client.system.run_shell_command_get_output(command)
        logging.info('Output >>> %s <<<', output)
        if checkfor and checkfor not in '\n'.join(output):
            raise error.TestFail('Expect %s in output of %s' %
                                 (checkfor, '\n'.join(output)))
        return output

    def _get_section(self, bios, section):
        """Return start address and size of an fmap section.

        @param bios: string, bios file name to retrieve fmap from
        @param section: string, section name to look for

        @return tuple of ints for start and size of the section.
        """
        # Store temp results in the attributes dictionary; when the expected
        # section name is found in the 'area_name:' field, the offset and size
        # of the section would be stored in this dictionary.
        attributes = {}
        for line in [x.strip() for x in self.run_cmd('dump_fmap %s' % bios)]:
            if not line:
                continue
            tokens = line.split()
            if tokens[0] == 'area_name:' and tokens[1] == section:
                return (int(attributes['area_offset:'], 16),
                        int(attributes['area_size:'], 16))

            if len(tokens) > 1:
                attributes[tokens[0]] = tokens[1]

        raise error.TestFail('Could not find section %s in the fmap' % section)

    def _create_test_blob(self, blob_name, blob_size):
        """On the DUT create a blob containing 0xff bytes of the requested size.

        @param blob_name: string, name of the DUT file to save the blob in
        @param blob_size: integer, size of the blob to prepare
        """
        test_blob_data = ('%c' % 0xff) * blob_size

        # Save it into a local file.
        handle, local_file = tempfile.mkstemp()

        try:
            os.write(handle, test_blob_data)
            os.close(handle)

            # Copy the local file to the DUT.
            self._client.send_file(local_file, blob_name)

        finally:
            # Delete local file.
            os.remove(local_file)

    def run_once(self, dev_mode=True):
        """Main method implementing test logic."""

        # Find out which AP firmware section is active to determine which AP
        # firmware section could be overwritten.
        active_fw = self.run_cmd('crossystem mainfw_act')[0]
        if active_fw == 'A':
            section = 'RW_SECTION_B'
        elif active_fw == 'B':
            section = 'RW_SECTION_A'
        else:
            raise error.TestFail('Unexpected active fw %s' % active_fw)

        dut_work_path = self.faft_client.updater.get_work_path()
        # Sizes to try to erase.
        test_sizes = (4096, 4096 * 2, 4096 * 4, 4096 * 8, 4096 * 16)

        # Image read from the AP firmware flash chip.
        bios_image = os.path.join(dut_work_path, 'bios_image')
        self.run_cmd('flashrom -r %s' % bios_image)

        # A blob of all ones to paste into the image.
        test_blob = os.path.join(dut_work_path, 'test_blob')
        self._create_test_blob(test_blob, max(test_sizes))

        # The file to store the 'corrupted' image with all ones pasted.
        junk_image = os.path.join(dut_work_path, 'junk_image')

        # Find in fmap the AP firmware section which can be overwritten.
        start, size = self._get_section(bios_image, section)
        logging.info('Active firmware %s, alternative at %#x:%#x', active_fw,
                     start, start + size -1)

        # Command to paste the all ones blob into the corrupted image.
        dd_template = 'dd if="%s" of="%s" bs=1 conv=notrunc seek="%d"' % (
            test_blob, junk_image, start)

        for test_size in test_sizes:

            logging.info('Verifying section of size %d', test_size)

            self.run_cmd('cp %s %s' % (bios_image, junk_image))

            # Set section in the 'junk' image to 'all erased'
            dd_cmd = dd_template + ' count="%d"' % test_size
            self.run_cmd(dd_cmd)

            # Now program the corrupted image, this would involve erasing the
            # section of test_size bytes.
            self.run_cmd('flashrom -w %s --diff %s --fast-verify' %
                         (junk_image, bios_image))

            # Now restore the image.
            self.run_cmd('flashrom -w %s --diff %s' % (bios_image, junk_image))
