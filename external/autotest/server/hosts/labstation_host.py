# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This file provides core logic for labstation verify/repair process."""

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.server.hosts import base_label
from autotest_lib.server.hosts import cros_label
from autotest_lib.server.cros import autoupdater
from autotest_lib.server.hosts import labstation_repair
from autotest_lib.server.cros import provision
from autotest_lib.server.hosts import base_servohost
from autotest_lib.client.cros import constants as client_constants
from autotest_lib.server.cros.dynamic_suite import constants as ds_constants
from autotest_lib.server.cros.dynamic_suite import tools
from autotest_lib.client.common_lib import lsbrelease_utils
from autotest_lib.client.common_lib.cros import dev_server
from autotest_lib.server import utils as server_utils


class LabstationHost(base_servohost.BaseServoHost):
    """Labstation specific host class"""

    # Threshold we decide to ignore a in_use file lock. In minutes
    IN_USE_FILE_EXPIRE_MINS = 120

    # Uptime threshold to perform a labstation reboot, this is to prevent a
    # broken DUT keep trying to reboot a labstation. In hours
    UP_TIME_THRESH_HOLD_HOURS = 24

    VERSION_PREFIX = provision.CROS_VERSION_PREFIX

    @staticmethod
    def check_host(host, timeout=10):
        """
        Check if the given host is a labstation host.

        @param host: An ssh host representing a device.
        @param timeout: The timeout for the run command.

        @return: True if the host device is labstation.

        @raises AutoservRunError: If the command failed.
        @raises AutoservSSHTimeout: Ssh connection has timed out.

        """
        try:
            result = host.run(
                'grep -q labstation /etc/lsb-release',
                ignore_status=True, timeout=timeout)
        except (error.AutoservRunError, error.AutoservSSHTimeout):
            return False
        return result.exit_status == 0


    def _initialize(self, hostname, *args, **dargs):
        super(LabstationHost, self)._initialize(hostname=hostname,
                                                *args, **dargs)
        self._repair_strategy = (
            labstation_repair.create_labstation_repair_strategy())
        self.labels = base_label.LabelRetriever(cros_label.LABSTATION_LABELS)
        logging.info('adding fake host_info_store to LabstationHost')
        try:
            host_info = self.host_info_store.get()
            host_info.stable_versions['servo-cros'] = host_info.stable_versions['cros']
            self.set_dut_host_info(host_info)
        except Exception as e:
            logging.exception(e)


    def is_reboot_requested(self):
        """Check if a reboot is requested for this labstation, the reboot can
        either be requested from labstation or DUTs. For request from DUTs we
        only process it if uptime longer than a threshold because we want
        to prevent a broken servo keep its labstation in reboot cycle.

        @returns True if a reboot is required, otherwise False
        """
        if self._check_update_status() == autoupdater.UPDATER_NEED_REBOOT:
            logging.info('Labstation reboot requested from labstation for'
                         ' update image')
            return True

        if not self._validate_uptime():
            logging.info('Ignoring DUTs reboot request because %s was'
                         ' rebooted in last 24 hours.', self.hostname)
            return False

        cmd = 'find %s*%s' % (self.TEMP_FILE_DIR, self.REBOOT_FILE_POSTFIX)
        output = self.run(cmd, ignore_status=True).stdout
        if output:
            in_use_file_list = output.strip().split('\n')
            logging.info('%s DUT(s) are currently requesting to'
                         ' reboot labstation.', len(in_use_file_list))
            return True
        else:
            return False


    def try_reboot(self):
        """Try to reboot the labstation if it's safe to do(no servo in use,
         and not processing updates), and cleanup reboot control file.
        """
        if (self._is_servo_in_use() or self._check_update_status()
            in autoupdater.UPDATER_PROCESSING_UPDATE):
            logging.info('Aborting reboot action because some DUT(s) are'
                         ' currently using servo(s) or'
                         ' labstation update is in processing.')
            return
        self._servo_host_reboot()
        logging.info('Cleaning up reboot control files.')
        self._cleanup_post_reboot()


    def get_labels(self):
        """Return the detected labels on the host."""
        return self.labels.get_labels(self)


    def get_os_type(self):
        return 'labstation'


    def prepare_for_update(self):
        """Prepares the DUT for an update.
        Subclasses may override this to perform any special actions
        required before updating.
        """
        pass


    def _get_lsb_release_content(self):
        """Return the content of lsb-release file of host."""
        return self.run(
            'cat "%s"' % client_constants.LSB_RELEASE).stdout.strip()


    def get_release_version(self):
        """Get the value of attribute CHROMEOS_RELEASE_VERSION from lsb-release.

        @returns The version string in lsb-release, under attribute
                 CHROMEOS_RELEASE_VERSION.
        """
        return lsbrelease_utils.get_chromeos_release_version(
            lsb_release_content=self._get_lsb_release_content())

    def verify_job_repo_url(self, tag=''):
        """
        Make sure job_repo_url of this host is valid.

        Eg: The job_repo_url "http://lmn.cd.ab.xyx:8080/static/\
        lumpy-release/R29-4279.0.0/autotest/packages" claims to have the
        autotest package for lumpy-release/R29-4279.0.0. If this isn't the case,
        download and extract it. If the devserver embedded in the url is
        unresponsive, update the job_repo_url of the host after staging it on
        another devserver.

        @param job_repo_url: A url pointing to the devserver where the autotest
            package for this build should be staged.
        @param tag: The tag from the server job, in the format
                    <job_id>-<user>/<hostname>, or <hostless> for a server job.

        @raises DevServerException: If we could not resolve a devserver.
        @raises AutoservError: If we're unable to save the new job_repo_url as
            a result of choosing a new devserver because the old one failed to
            respond to a health check.
        @raises urllib2.URLError: If the devserver embedded in job_repo_url
                                  doesn't respond within the timeout.
        """
        info = self.host_info_store.get()
        job_repo_url = info.attributes.get(ds_constants.JOB_REPO_URL, '')
        if not job_repo_url:
            logging.warning('No job repo url set on host %s', self.hostname)
            return

        logging.info('Verifying job repo url %s', job_repo_url)
        devserver_url, image_name = tools.get_devserver_build_from_package_url(
            job_repo_url)

        ds = dev_server.ImageServer(devserver_url)

        logging.info('Staging autotest artifacts for %s on devserver %s',
                     image_name, ds.url())

        ds.stage_artifacts(image_name, ['autotest_packages'])


    def host_version_prefix(self, image):
        """Return version label prefix.

        In case the CrOS provisioning version is something other than the
        standard CrOS version e.g. CrOS TH version, this function will
        find the prefix from provision.py.

        @param image: The image name to find its version prefix.
        @returns: A prefix string for the image type.
        """
        return provision.get_version_label_prefix(image)


    def stage_server_side_package(self, image=None):
        """Stage autotest server-side package on devserver.

        @param image: Full path of an OS image to install or a build name.

        @return: A url to the autotest server-side package.

        @raise: error.AutoservError if fail to locate the build to test with, or
                fail to stage server-side package.
        """
        # If enable_drone_in_restricted_subnet is False, do not set hostname
        # in devserver.resolve call, so a devserver in non-restricted subnet
        # is picked to stage autotest server package for drone to download.
        hostname = self.hostname
        if not server_utils.ENABLE_DRONE_IN_RESTRICTED_SUBNET:
            hostname = None
        if image:
            image_name = tools.get_build_from_image(image)
            if not image_name:
                raise error.AutoservError(
                    'Failed to parse build name from %s' % image)
            ds = dev_server.ImageServer.resolve(image_name, hostname)
        else:
            info = self.host_info_store.get()
            job_repo_url = info.attributes.get(ds_constants.JOB_REPO_URL, '')
            if job_repo_url:
                devserver_url, image_name = (
                    tools.get_devserver_build_from_package_url(job_repo_url))
                # If enable_drone_in_restricted_subnet is True, use the
                # existing devserver. Otherwise, resolve a new one in
                # non-restricted subnet.
                if server_utils.ENABLE_DRONE_IN_RESTRICTED_SUBNET:
                    ds = dev_server.ImageServer(devserver_url)
                else:
                    ds = dev_server.ImageServer.resolve(image_name)
            elif info.build is not None:
                ds = dev_server.ImageServer.resolve(info.build, hostname)
                image_name = info.build
            else:
                raise error.AutoservError(
                    'Failed to stage server-side package. The host has '
                    'no job_repo_url attribute or cros-version label.')

        ds.stage_artifacts(image_name, ['autotest_server_package'])
        return '%s/static/%s/%s' % (ds.url(), image_name,
                                    'autotest_server_package.tar.bz2')


    def repair(self):
        """Attempt to repair a labstation.
        """
        message = 'Beginning repair for host %s board %s model %s'
        info = self.host_info_store.get()
        message %= (self.hostname, info.board, info.model)
        self.record('INFO', None, None, message)
        self._repair_strategy.repair(self)


    def _validate_uptime(self):
        return (float(self.check_uptime()) >
                self.UP_TIME_THRESH_HOLD_HOURS * 3600)


    def _is_servo_in_use(self):
        """Determine if there are any DUTs currently running task that uses
         servo, only files that has been touched within pre-set threshold of
          minutes counts.

        @returns True if any DUTs is using servos, otherwise False.
        """
        cmd = 'find %s*%s -mmin -%s' % (self.TEMP_FILE_DIR,
                                        self.LOCK_FILE_POSTFIX,
                                        self.IN_USE_FILE_EXPIRE_MINS)
        result = self.run(cmd, ignore_status=True)
        return bool(result.stdout)


    def _cleanup_post_reboot(self):
        """Clean up all xxxx_reboot file after reboot."""
        cmd = 'rm %s*%s' % (self.TEMP_FILE_DIR, self.REBOOT_FILE_POSTFIX)
        self.run(cmd, ignore_status=True)
