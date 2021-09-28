import json
import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_utils
from autotest_lib.client.cros.enterprise.policy_group import AllPolicies


CHROMEPOLICIES = 'chromePolicies'
DEVICELOCALACCOUNT = 'deviceLocalAccountPolicies'
EXTENSIONPOLICIES = 'extensionPolicies'

CHROMEOS_CLOUDDPC = {
    'AudioCaptureAllowed': 'unmuteMicrophoneDisabled',
    'DefaultGeolocationSetting': 'shareLocationDisabled',
    'DeviceBlockDevmode': 'debuggingFeaturesDisabled',
    'DisableScreenshots': 'screenCaptureDisabled',
    'ExternalStorageDisabled': 'usbFileTransferDisabled',
    'VideoCaptureAllowed': 'cameraDisabled',
}


class Policy_Manager(object):

    def __init__(self, username=None, fake_dm_server=None):
        """
        This class is to hanlde:
            Setting policies to a Fake DM server
            Obtaining policies from a DUT
            Obtaining clouddpc policy settings.
            Obtinaing both set/received policies in many different data formats
            Verifying policies provided to the DUT are correct.

        It has been designed so all the features are independent, meaning a
        fake_dm_server is not required to obtain policies from the DUT, get
        clouddpc values, etc.

        @params username: username to be used when creating the fake DM policy.
        @params fake_dm_server: The fake DM server object.

        """
        self._configured = AllPolicies(True)
        self._obtained = AllPolicies()
        self.CHROME = 'chrome'
        self.LOCAL = 'local'
        self.EXT = 'ext'
        self.username = username
        self.fake_dm_server = fake_dm_server
        self.autotest_ext = None

        # If a fake_dm_sever is provided, enabled auto_update by default.
        # Can be turned off if desired.
        self._auto_updateDM = bool(fake_dm_server)

    def configure_policies(self,
                           user={},
                           suggested_user={},
                           device={},
                           extension={},
                           new=True):
        """
        Used to configure desired policies on the DUT. Will also save the
        configured policies.

        If a fake_dm_server was provided on class initialization, and
        auto_updateDM is not False, this will also update the fake_dm_server.

        @params user/suggested_user/device/extension: dict of the policies to
            be set. extension must be provided with the extension id. e.g.
            {'ExtensionID': {policy_dict}}
        @param new: bool, if True clear all previously configured policies.

        """
        if new:
            self._configured = AllPolicies(True)

        self._configured.set_policy('chrome', user, 'user')
        self._configured.set_policy('chrome', suggested_user, 'suggested_user')
        self._configured.set_policy('chrome', device, 'device')

        self._configured.set_extension_policy(extension)

        if self.auto_updateDM:
            self.updateDMServer()

    def configure_extension_visual_policy(self,
                                          ext_policy={},
                                          new=True):
        """
        Extensions are... different. The policy set for them often is not the
        actual policy, but a pointer to where the policy data is. This makes
        verifying the extension policy tricky.

        To help with, this function will allow you to set the 'real' policy.
        Important things to note: This is only useful for verfying the policy,
        and getting the policy as a dictionary (which has a flag for which
        style of extension policies you want to see. The DM server will not be
        set via this.

        @param ext_policy: dict, Extension policy must be provided with the
            extension id:
                {'ExtensionID': {policy_dict}}.
        @param new: bool, if True will erase any previously stored VISUAL
            extension data.

        """
        if new:
            self._configured.ext_values = {}

        self._configured.set_extension_policy(ext_policy, True)

    def remove_policy(self,
                      policy_name,
                      policy_type,
                      extID=None):
        """
        Removes the policy from the configured policies. Useful when you want
        clear a specific policy, but leave the other policies untouched.
        If auto_updateDM is True (thus a fake_dm_server has been provided), the
        dm server will be updated.

        @param policy_name: The policy name
        @param policy_type: The type of policy it is. Valid types:
            "user", "device", "extension", "suggested_user".
        @param extID: The extension ID, if removing an extension policy.

        """
        if policy_type != 'extension':
            self._removeChromePolicy(policy_name)
        else:
            self._removeExtensionPolicy(policy_name, extID)

        if self.auto_updateDM:
            self.updateDMServer()

    def _removeChromePolicy(self, policy_name):
        """
        Attempts to remove the specified extension policy from specified
        extension.

        @rasies error.TestError: If the policy is not in the configured
            policies.

        """
        try:
            del self._configured.chrome[policy_name]
        except KeyError:
            raise error.TestError('Policy {} missing from chrome policies.'
                                  .format(policy_name))

    def _removeExtensionPolicy(self, policy_name, extID):
        """
        Attempts to remove the specified extension policy from specified
        extension.

        @raises error.TestError: if the policy_type is an 'extension', but the
            extID is not provided, or the policy is not found in the extension.

        """
        if not extID:
            raise error.TestError(
                'Cannot delete extension policy without extension ID')
        try:
            del self._configured.extension_configured_data[extID][policy_name]
        except KeyError:
            raise error.TestError(
                'Policy {} missing from extension policies.'
                .format(policy_name))

    def obtain_policies_from_device(self, autotest_ext=None):
        """
        Calls the autotest private getAllEnterprisePolicies() API, and saves
        the response.

        @param autotest_ext: The autotest browser extension.

        """
        if autotest_ext:
            self.autotest_ext = autotest_ext
        if not self.autotest_ext:
            raise error.TestError('Cannot obtain policies without autotest_ext')
        self.raw_data = enterprise_policy_utils.get_all_policies(
            self.autotest_ext)
        self._obtained.set_policy(self.CHROME, self.raw_data[CHROMEPOLICIES])
        self._obtained.set_policy(self.LOCAL, self.raw_data[DEVICELOCALACCOUNT])
        self._obtained.set_extension_policy(self.raw_data[EXTENSIONPOLICIES])

    def verify_policy(self, policyName, policy_value, extID=None):
        """Verifies the configured policies are == to the policies obtained."""
        recieved_value = self.get_policy_value_from_DUT(policyName=policyName,
                                                        extID=extID,
                                                        refresh=True)
        if not recieved_value == policy_value:
            raise error.TestError(
                'Policy {} value was not set correctly. \nExpected:\t {}'
                '\nReceived: \t'.format(policyName,
                                        policy_value,
                                        recieved_value))
        logging.info('Policy verification successful')

    def verify_policies(self):
        """Verifies the configured policies are == to the policies obtained."""
        if not self._configured == self._obtained:
            raise error.TestError(
                'Configured policies did not match policies received from DUT.')
        logging.info('Policy verification successful')

    def get_policy_value_from_DUT(self, policyName, extID=None, refresh=False):
        """
        Get the value of a specified policy from the DUT. If the policy is from
        an extension, an extension ID (extID) must be provided.

        @param policyName: str, the name of the policy.
        @param extID: The ID of the extension.
        @param refresh: bool, if you want to get the policies from the DUT again
            Note: This does NOT force the DUT to re-obtain the policies from the
            DM server.

        @returns: The value of the policy if found, else None.
        """
        if refresh:
            # Uses the previously provided autotest Extension.
            self.obtain_policies_from_device()

        if extID:
            if policyName in self._obtained.extension_configured_data[extID]:
                return (
                    self._obtained.extension_configured_data[extID][policyName]
                        .value)
        elif policyName in self._obtained.chrome:
            return self._obtained.chrome[policyName].value
        return None

    def updateDMServer(self):
        """Updates the Fake DM server with the current configured policy."""
        fake_dm_json = self.getDMConfig()
        logging.info('Policy blob {}'.format(fake_dm_json))
        if not self.fake_dm_server:
            raise error.TestError(
                'Cannot update DM server. DM server not provided')
        self.fake_dm_server.setup_policy(fake_dm_json)

    def getDMConfig(self, refresh=True):
        """"
        Creates the DM configuration (aka Json blob) to be used by the
        fake DM server

        @param refresh: bool, if True, will clear any previous configuration.
            If False, return the currently configured DM blob.

        """
        if refresh:
            self._configured.createNewFakeDMServerJson()
        self._configured.updateDMJson()
        self._configured._DMJSON['policy_user'] = self.username
        return json.dumps(self._configured._DMJSON)

    def getCloudDpc(self):
        """Gets the Cloud DPC ARC policy settings."""
        expected_cloud_dpc_settings = {}
        if self._arc_certs():
            self._add_arc_certs(expected_cloud_dpc_settings)
        self._add_shared_policies(expected_cloud_dpc_settings)
        self._add_shared_arc_policy(expected_cloud_dpc_settings)

        return expected_cloud_dpc_settings

    def _arc_certs(self):
        """
        Returns True if ArcCertificatesSyncMode is set in the configured
        policies and the bool(value) is True, else False.

        """
        if ('ArcCertificatesSyncMode' in self._configured.chrome and
                self._configured.chrome['ArcCertificatesSyncMode'].value):
            return True
        return False

    def _add_shared_arc_policy(self, dpc):
        """Adds the shared policies that are subset within the 'ArcPolicy'."""
        Arc_Policy = self._configured.chrome.get('ArcPolicy', {})
        if Arc_Policy:
            Arc_Policy = Arc_Policy.value

        for key in ['applications', 'accountTypesWithManagementDisabled']:
            if key in Arc_Policy:
                dpc[key] = Arc_Policy[key]

    def _add_shared_policies(self, dpc):
        """
        Add all of the configured policies that are shared with arc clouddpc,
        to the "dpc" dict, with the clouddpc key. If the policy is not set, it
        will not be added.

        """
        for policy_name, dpc_name in CHROMEOS_CLOUDDPC.items():
            if policy_name in self._configured.chrome:
                dpc[dpc_name] = self._configured.chrome[policy_name].value

    def _add_arc_certs(self, dpc):
        open_network_config = 'OpenNetworkConfiguration'
        if open_network_config in self._configured.chrome:
            dpc['caCerts'] = self._configured.chrome[open_network_config].value
        else:
            dpc['caCerts'] = None

    def get_configured_policies_as_dict(self, visual=False):
        """ Returns the configured policies as a dict."""
        return self._configured.get_policy_as_dict(visual)

    def get_obtained_policies_as_dict(self):
        """ Returns the obtained policies as a dict."""
        return self._obtained.get_policy_as_dict(visual=True)

    @property
    def auto_updateDM(self):
        """ Returns the current state of the auto_updateDM setting."""
        return self._auto_updateDM

    @auto_updateDM.setter
    def auto_updateDM(self, value):
        """Turns on/off auto updating of the DM server."""
        if not isinstance(value, bool):
            raise error.TestError('Auto Update DM must be bool, got {}'
                                   .format(value))
        if value and not self.fake_dm_server:
            raise error.TestError(
                'Cannot autoupdate without the Fake DM server configured.')
        self._auto_updateDM = value
