from multiprocessing import Process
import time

from acts.test_utils.bt.A2dpBaseTest import A2dpBaseTest

END_TOKEN = "end"


class BtInterferenceRSSITest(A2dpBaseTest):
    """Test that streams audio from Android phone to relay controlled headset
    over Bluetooth while running command sequences on one or more attenuators.

    The command sequences are passed in a config parameter "bt_atten_sequences".

    bt_atten_sequences: a dictionary mapping attenuator device index to a list
        of action strings. These action strings are specific to this test and
        take the form "<action>:<param>" where <action> corresponds to a method
        name for this test class and <param> is the single value arg to pass to
        that method.
        Example: {"0": ["set:50", "wait_seconds:2", "set:0", "wait_seconds:2"],
                  "1": ["set:10", "end"]}

            The above dictionary would toggle attenuator at 0 to 50 dB for 2 sec
            and back to 0 dB for 2 sec in a loop, while simultaneously setting
            attenuator at 1 to 10dB. Passing the END_TOKEN (defined in this
            module) terminates the sequence and keeps it from looping.
    """

    def setup_class(self):
        super().setup_class()
        req_params = ["bt_atten_sequences", "RelayDevice", "codec"]
        opt_params = ["audio_params"]
        self.unpack_userparams(req_params, opt_params)

    def set(self, attenuator, value):
        self.log.debug("Set attenuator %s to %s" % (attenuator.idx, value))
        attenuator.set_atten(int(value))

    def wait_seconds(self, attenuator, value):
        self.log.debug("Attenuator %s wait for %s seconds" %
                       (attenuator.idx, value))
        time.sleep(float(value))

    def atten_sequence_worker(self, attenuator, action_sequence):
        while True:
            for action in action_sequence:
                if action == END_TOKEN:  # Stop the sequence and don't loop
                    return
                method, value = action.split(":")
                getattr(self, method)(attenuator, value)

    def set_all_attenuators(self, value):
        for attenuator in self.attenuators:
            attenuator.set_atten(value)

    def teardown_class(self):
        self.set_all_attenuators(0)
        super().teardown_class()

    def test_multi_atten_streaming(self):
        self.set_all_attenuators(0)
        # Connect phone and headset before setting any attenuators.
        self.connect_with_retries(retries=5)
        # Create processes for streaming and attenuating to run in parallel.
        stream_and_record_proc = Process(target=self.stream_music_on_codec,
                                         kwargs=self.codec)
        attenuation_processes = []
        for channel, action_sequence in self.bt_atten_sequences.items():
            process = Process(target=self.atten_sequence_worker,
                              args=(self.attenuators[int(channel)],
                                    action_sequence))
            attenuation_processes.append(process)
        stream_and_record_proc.start()
        for process in attenuation_processes:
            process.start()
        stream_and_record_proc.join()
        for process in attenuation_processes:
            process.terminate()