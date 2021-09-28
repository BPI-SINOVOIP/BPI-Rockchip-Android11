import time
from acts import utils
from acts import signals
from acts.base_test import BaseTestClass
from acts.test_utils.tel.tel_defines import EventSmsSentSuccess
from acts.test_utils.tel.tel_test_utils import get_iccid_by_adb
from acts.test_utils.tel.tel_test_utils import is_sim_ready_by_adb


class GnssSimInventoryTest(BaseTestClass):
    """ GNSS SIM Inventory Tests"""
    def setup_class(self):
        super().setup_class()
        self.ad = self.android_devices[0]
        req_params = ["sim_inventory_recipient", "sim_inventory_ldap"]
        self.unpack_userparams(req_param_names=req_params)

    def check_device_status(self):
        if int(self.ad.adb.shell("settings get global airplane_mode_on")) != 0:
            self.ad.log.info("Force airplane mode off")
            utils.force_airplane_mode(self.ad, False)
        if not is_sim_ready_by_adb(self.ad.log, self.ad):
            raise signals.TestFailure("SIM card is not loaded and ready.")

    def test_gnss_sim_inventory(self):
        self.check_device_status()
        imsi = str(self.ad.adb.shell("service call iphonesubinfo 7"))
        if not imsi:
            raise signals.TestFailure("Couldn't get imsi")
        iccid = str(get_iccid_by_adb(self.ad))
        if not iccid:
            raise signals.TestFailure("Couldn't get iccid")
        sms_message = "imsi: %s, iccid: %s, ldap: %s, model: %s, sn: %s" % \
                      (imsi, iccid, self.sim_inventory_ldap, self.ad.model,
                       self.ad.serial)
        self.ad.log.info(sms_message)
        try:
            self.ad.log.info("Send SMS by SL4A.")
            self.ad.droid.smsSendTextMessage(self.sim_inventory_recipient,
                                             sms_message, True)
            self.ad.ed.pop_event(EventSmsSentSuccess, 10)
        except Exception as e:
            raise signals.TestFailure(e)
