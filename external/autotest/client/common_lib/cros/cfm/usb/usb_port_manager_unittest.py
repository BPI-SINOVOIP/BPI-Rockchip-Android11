import mock
import unittest

from autotest_lib.client.common_lib.cros.cfm.usb import usb_port_manager

# gpio for the port on bus 1, port 2. Specified in
# usb_port_manager._PORT_ID_TO_GPIO_INDEX_DICT
GUADO_GPIO = 218
GUADO_GPIO_PATH = '/sys/class/gpio/gpio%s' % GUADO_GPIO
FIZZ_GPIO = 3

# pylint: disable=missing-docstring
class UsbPortManagerTest(unittest.TestCase):

    def test_power_off_gpio_unexported_guado(self):
        host = mock.Mock()
        host.get_board = mock.Mock(return_value='board: guado')
        host.run = mock.Mock()
        host.path_exists = mock.Mock(return_value=False)
        port_manager = usb_port_manager.UsbPortManager(host)
        port_manager.set_port_power([(1, 2)], False)
        expected_runs = [
                mock.call('echo %s > /sys/class/gpio/export' %  GUADO_GPIO),
                mock.call('echo out > %s/direction' % GUADO_GPIO_PATH),
                mock.call('echo 0 > %s/value' % GUADO_GPIO_PATH),
                mock.call('echo %s > /sys/class/gpio/unexport' %  GUADO_GPIO),
        ]
        host.run.assert_has_calls(expected_runs)

    def test_power_on_gpio_exported_guado(self):
        host = mock.Mock()
        host.get_board = mock.Mock(return_value='board: guado')
        host.run = mock.Mock()
        host.path_exists = mock.Mock(return_value=True)
        port_manager = usb_port_manager.UsbPortManager(host)
        port_manager.set_port_power([(1, 2)], True)
        expected_runs = [
                mock.call('echo out > %s/direction' % GUADO_GPIO_PATH),
                mock.call('echo 1 > %s/value' % GUADO_GPIO_PATH),
        ]
        host.run.assert_has_calls(expected_runs)

    def test_power_off_gpio_fizz(self):
        host = mock.Mock()
        host.get_board = mock.Mock(return_value='board: fizz')
        host.run = mock.Mock()
        port_manager = usb_port_manager.UsbPortManager(host)
        port_manager.set_port_power([(1, 2)], False)
        expected_runs = [
                mock.call('ectool gpioset USB%s_ENABLE 0' % FIZZ_GPIO)
        ]
        host.run.assert_has_calls(expected_runs)

    def test_power_on_gpio_fizz(self):
        host = mock.Mock()
        host.get_board = mock.Mock(return_value='board: fizz')
        host.run = mock.Mock()
        port_manager = usb_port_manager.UsbPortManager(host)
        port_manager.set_port_power([(1, 2)], True)
        expected_runs = [
                mock.call('ectool gpioset USB%s_ENABLE 1' % FIZZ_GPIO)
        ]
        host.run.assert_has_calls(expected_runs)
