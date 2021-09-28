# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import test

# TPM vendor commands have the following header structure:

# 8001      TPM_ST_NO_SESSIONS
# 00000000  Command/response size
# 20000000  Cr50 Vendor Command (Constant, TPM Command Code)
# 0000      Vendor Command Code (VENDOR_CC_ enum)

TPM_TAG_SIZE_BYTES = 2
VENDOR_CC_SIZE_BYTES = 2
VENDOR_CMD_HEADER_SIZE_BYTES = 12

# Responses to TPM vendor commands have the following header structure:

# 8001      TPM_ST_NO_SESSIONS
# 00000000  Response size
# 00000000  Response code
# 0000      Vendor Command Code

VENDOR_CMD_RESPONSE_SIZE_OFFSET = TPM_TAG_SIZE_BYTES
VENDOR_CMD_RESPONSE_SIZE_BYTES = 4
VENDOR_CMD_RESPONSE_CODE_OFFSET = (
    VENDOR_CMD_RESPONSE_SIZE_OFFSET + VENDOR_CMD_RESPONSE_SIZE_BYTES)
VENDOR_CMD_RESPONSE_CODE_SIZE_BYTES = 4
VENDOR_CMD_RESPONSE_CC_OFFSET = (
    VENDOR_CMD_RESPONSE_CODE_OFFSET + VENDOR_CMD_RESPONSE_CODE_SIZE_BYTES)

# Vendor command codes being tested

VENDOR_CC_U2F_GENERATE = '002C'
VENDOR_CC_U2F_SIGN = '002D'
VENDOR_CC_U2F_ATTEST = '002E'

# Expected response sizes (body only)

VENDOR_CC_U2F_SIGN_RESPONSE_SIZE_BYTES = 64
VENDOR_CC_U2F_GENERATE_RESPONSE_SIZE_BYTES = 129
VENDOR_CC_U2F_ATTEST_RESPONSE_SIZE_BYTES = 64

# Response Codes

VENDOR_CMD_RESPONSE_SUCCESS = '00000000'
VENDOR_CMD_RESPONSE_BOGUS_ARGS = '00000501'
VENDOR_CMD_RESPONSE_NOT_ALLOWED = '00000507'
VENDOR_CMD_RESPONSE_PASSWORD_REQUIRED = '0000050A'

# U2F Attest constants

U2F_ATTEST_FORMAT_REG_RESP = '00'
U2F_ATTEST_REG_RESP_SIZE_BYTES = 194

# Some 'random' input to simulate actual inputs.
APP_ID = '699abb209a23ec31dcef298064a92ed9829e70a1bc873b272db321fe1644feae'
APP_ID_2 = '3c67e46408ec57dc6e4fb46fd0aecddadcf10c7b856446986ef67544a00530fa'
USER_SECRET_1 = ('1b6e854dcc052dfff2b5ece48c60a9db'
                 'c69d27315c5f3ef8031abab60aa24d61')
USER_SECRET_2 = ('26398186431b14de9a6b99f849d71d342'
                 'a1ec246d413aed42b7f2ac98846f24d')
HASH_TO_SIGN = ('91f93c8d88ed6168d07a36de53bd62b6'
                '649e84d343dd417ed6062775739b6e65')
RANDOM_32 = '0fd2bf886fa8c036d069adf321bf1390859da4d615034c3a81ca3812a210ce0d'


def get_bytes(tpm_str, start, length):
  return tpm_str[(start * 2):(start * 2 + length * 2)]


def assert_byte_length(str, len_bytes):
  """Assert str represents a byte sequence len_bytes long"""
  assert (len(str) / 2) == len_bytes


def get_str_length_as_hex(str, additional_len=0):
  """Get the length of str plus any additional_len as a hex string."""
  assert (len(str) % 2) == 0
  length_bytes = len(str) / 2
  # hex() returns strings with a '0x' prefix, which we remove.
  return hex(length_bytes + additional_len)[2:]


def check_response_size(response, expected_response, success_size):
  """If the response is expected to be success, check it's size is as expected,

     otherwise, check it is 0.
  """
  response_size = response['length']
  if expected_response == VENDOR_CMD_RESPONSE_SUCCESS:
    if response_size != success_size:
      raise error.TestFail(
          'Invalid successful response size: {}'.format(response_size))
  elif response_size != 0:
    raise error.TestFail(
        'Non-zero response size on failure: {}'.format(response_size))


class firmware_Cr50U2fCommands(test.test):
  """Tests the custom U2F commands in cr50"""

  version = 1

  def __send_vendor_cmd(self,
                        vendor_cc,
                        cmd_body,
                        expected_response_code=VENDOR_CMD_RESPONSE_SUCCESS):
    assert_byte_length(vendor_cc, VENDOR_CC_SIZE_BYTES)

    cmd_size_str = get_str_length_as_hex(cmd_body, VENDOR_CMD_HEADER_SIZE_BYTES)

    cmd = (
        '8001'  # TPM_ST_NO_SESSIONS
        '{:0>8}'  # Command Size (UINT32)
        '20000000'  # CR50 Vendor Command (TPM CC)
        '{}'  # Vendor Command Code (Subcommand Code, UINT16)
        '{}'  # Command Body
    ).format(cmd_size_str, vendor_cc, cmd_body)

    result = self.client.run('trunks_send --raw {}'.format(cmd)).stdout.strip()

    if get_bytes(result, 0, TPM_TAG_SIZE_BYTES) != '8001':
      raise error.TestFail(
          'Unexpected response tag from vendor command: {}'.format(result))

    response_size_bytes = int(
        get_bytes(result, VENDOR_CMD_RESPONSE_SIZE_OFFSET,
                  VENDOR_CMD_RESPONSE_SIZE_BYTES), 16)

    if response_size_bytes < VENDOR_CMD_HEADER_SIZE_BYTES:
      raise error.TestFail(
          'Unexpected response length from vendor command: {}'.format(result))

    response_code = get_bytes(result, VENDOR_CMD_RESPONSE_CODE_OFFSET,
                              VENDOR_CMD_RESPONSE_CODE_SIZE_BYTES)

    if response_code != expected_response_code:
      raise error.TestFail(
          'Unexpected response received from vendor command: {}'.format(
              response_code))

    response_vendor_cc = get_bytes(result, VENDOR_CMD_RESPONSE_CC_OFFSET,
                                   VENDOR_CC_SIZE_BYTES)

    if response_vendor_cc != vendor_cc:
      raise error.TestFail(
          'Received response for unexpected vendor command code: {}'.format(
              response_vendor_cc))

    response_body_size_bytes = (
        response_size_bytes - VENDOR_CMD_HEADER_SIZE_BYTES)

    return {
        'length':
            response_body_size_bytes,
        'value':
            get_bytes(result, VENDOR_CMD_HEADER_SIZE_BYTES,
                      response_body_size_bytes)
    }

  def __u2f_sign(self, app_id, user_secret, key_handle, hash, flags,
                 expected_response):
    assert_byte_length(app_id, 32)
    assert_byte_length(user_secret, 32)
    assert_byte_length(key_handle, 64)
    assert_byte_length(flags, 1)

    response = self.__send_vendor_cmd(
        VENDOR_CC_U2F_SIGN, '{}{}{}{}{}'.format(app_id, user_secret, key_handle,
                                                hash, flags), expected_response)

    expected_response_size = VENDOR_CC_U2F_SIGN_RESPONSE_SIZE_BYTES
    # 'check-only' requests don't have a response body.
    if flags == '07':
      expected_response_size = 0

    check_response_size(response, expected_response, expected_response_size)

  def __u2f_generate(self,
                     app_id,
                     user_secret,
                     flags,
                     expected_response=VENDOR_CMD_RESPONSE_SUCCESS):
    assert_byte_length(app_id, 32)
    assert_byte_length(user_secret, 32)
    assert_byte_length(flags, 1)

    response = self.__send_vendor_cmd(
        VENDOR_CC_U2F_GENERATE, '{}{}{}'.format(app_id, user_secret, flags),
        expected_response)

    check_response_size(response, expected_response,
                        VENDOR_CC_U2F_GENERATE_RESPONSE_SIZE_BYTES)

    return {
        'pubKey': response['value'][0:130],
        'keyHandle': response['value'][130:258]
    }

  def __u2f_attest(self,
                   user_secret,
                   format,
                   data,
                   expected_response=VENDOR_CMD_RESPONSE_SUCCESS,
                   pad=False,
                   truncated=False):
    assert_byte_length(user_secret, 32)
    assert_byte_length(format, 1)

    data_len_str = get_str_length_as_hex(data)

    if truncated:
      # Send 1 less byte of data than will be advertised in data_len field
      assert pad == False
      assert len(data) >= 2
      data = data[:len(data) - 2]

    if pad:
      # Max data size is 256 bytes
      data = data + '0' * (512 - len(data))

    response = self.__send_vendor_cmd(
        VENDOR_CC_U2F_ATTEST, '{}{}{}{}'.format(
            user_secret, format, data_len_str, data), expected_response)

    check_response_size(response, expected_response,
                        VENDOR_CC_U2F_ATTEST_RESPONSE_SIZE_BYTES)

  def __test_generate_unique(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')
    registration_2 = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    if registration['pubKey'] == registration_2['pubKey']:
      raise error.TestFail('Public keys not unique')

    if registration['keyHandle'] == registration_2['keyHandle']:
      raise error.TestFail('Key handles not unique')

  def __test_generate_sign_simple(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.servo.power_short_press()

    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_SUCCESS)

  def __test_generate_with_presence(self):
    # Wait 11 seconds to ensure no presence.

    time.sleep(11)

    self.__u2f_generate(
        APP_ID,
        USER_SECRET_1,
        '01',  # U2F_AUTH_FLAG_TUP
        VENDOR_CMD_RESPONSE_NOT_ALLOWED)

    self.servo.power_short_press()

    self.__u2f_generate(
        APP_ID,
        USER_SECRET_1,
        '01',  # U2F_AUTH_FLAG_TUP
        VENDOR_CMD_RESPONSE_SUCCESS)

  def __test_generate_consume_presence(self):
    self.servo.power_short_press()

    self.__u2f_generate(
        APP_ID,
        USER_SECRET_1,
        '03',  # U2F_AUTH_FLAG_TUP | G2F_CONSUME
        VENDOR_CMD_RESPONSE_SUCCESS)

    self.__u2f_generate(
        APP_ID,
        USER_SECRET_1,
        '01',  # U2F_AUTH_FLAG_TUP
        VENDOR_CMD_RESPONSE_NOT_ALLOWED)

  def __test_sign_requires_presence(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    # U2F asserts presence by checking for a power button press within the
    # last 10 seconds, sleep so that we are sure there was not one.

    time.sleep(11)

    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_NOT_ALLOWED)

  def __test_sign_multiple_no_consume(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.servo.power_short_press()

    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_SUCCESS)

    # We should be able to sign again, as this will happen within 10
    # seconds of the power button press, and we did not consume.

    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_SUCCESS)

  def __test_sign_consume(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.servo.power_short_press()

    self.__u2f_sign(
        APP_ID,
        USER_SECRET_1,
        registration['keyHandle'],
        HASH_TO_SIGN,
        '02',  # G2F_CONSUME
        VENDOR_CMD_RESPONSE_SUCCESS)

    # We should have consumed the power button press, so we should not be
    # able to sign again.

    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_NOT_ALLOWED)

  def __test_sign_wrong_user_secret(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.servo.power_short_press()

    # Sanity check.
    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_SUCCESS)

    self.__u2f_sign(APP_ID, USER_SECRET_2, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_PASSWORD_REQUIRED)

  def __test_sign_wrong_app_id(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.servo.power_short_press()

    # Sanity check.
    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_SUCCESS)

    self.__u2f_sign(APP_ID_2, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_PASSWORD_REQUIRED)

  def __test_sign_invalid_kh(self):
    # U2F asserts presence by checking for a power button press within the
    # last 10 seconds, sleep so that we are sure there was not one.

    time.sleep(11)

    self.__u2f_sign(
        APP_ID,
        USER_SECRET_1,
        RANDOM_32 + RANDOM_32,  # KH is 64 bytes long
        HASH_TO_SIGN,
        '00',
        VENDOR_CMD_RESPONSE_PASSWORD_REQUIRED)

    self.__u2f_sign(
        APP_ID,
        USER_SECRET_1,
        RANDOM_32 + RANDOM_32,  # KH is 64 bytes long
        HASH_TO_SIGN,
        '02',  # G2F_CONSUME
        VENDOR_CMD_RESPONSE_PASSWORD_REQUIRED)

  def __test_sign_invalid_kh_with_presence(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.servo.power_short_press()

    # Should return invalid KH error, without consuming presence.
    self.__u2f_sign(
        APP_ID,
        USER_SECRET_1,
        RANDOM_32 + RANDOM_32,  # KH is 64 bytes long
        HASH_TO_SIGN,
        '02',  # G2F_CONSUME
        VENDOR_CMD_RESPONSE_PASSWORD_REQUIRED)

    # Check presence was not consumed.
    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_SUCCESS)

  def __test_sign_check_only(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    # U2F asserts presence by checking for a power button press within the
    # last 10 seconds, sleep so that we are sure there was not one.

    time.sleep(11)

    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '07', VENDOR_CMD_RESPONSE_SUCCESS)

  def __test_sign_check_only_with_presence(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.servo.power_short_press()

    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '07', VENDOR_CMD_RESPONSE_SUCCESS)

  def __test_sign_check_only_invalid_kh(self):
    # U2F asserts presence by checking for a power button press within the
    # last 10 seconds, sleep so that we are sure there was not one.

    time.sleep(11)

    self.__u2f_sign(APP_ID,
                    USER_SECRET_1,
                    RANDOM_32 + RANDOM_32,  # KH is 64 bytes long
                    HASH_TO_SIGN,
                    '07',
                    VENDOR_CMD_RESPONSE_PASSWORD_REQUIRED)

  def __test_sign_check_only_invalid_kh_with_presence(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.servo.power_short_press()

    self.__u2f_sign(APP_ID,
                    USER_SECRET_1,
                    RANDOM_32 + RANDOM_32,  # KH is 64 bytes long
                    HASH_TO_SIGN,
                    '07',
                    VENDOR_CMD_RESPONSE_PASSWORD_REQUIRED)

  def __check_attest_reg_resp(self,
                              app_id,
                              key_handle,
                              pub_key,
                              user_secret,
                              expected_response,
                              pad=False):
    register_resp = '00{}{}{}{}'.format(
        app_id,
        RANDOM_32,  # challenge
        key_handle,
        pub_key)

    self.__u2f_attest(user_secret, U2F_ATTEST_FORMAT_REG_RESP, register_resp,
                      expected_response, pad)

  def __test_attest_simple(self):
    # Attest does not require user presence
    time.sleep(11)

    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.__check_attest_reg_resp(APP_ID, registration['keyHandle'],
                                 registration['pubKey'], USER_SECRET_1,
                                 VENDOR_CMD_RESPONSE_SUCCESS)

  def __test_attest_simple_padded(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.__check_attest_reg_resp(
        APP_ID,
        registration['keyHandle'],
        registration['pubKey'],
        USER_SECRET_1,
        VENDOR_CMD_RESPONSE_SUCCESS,
        pad=True)

  def __test_attest_wrong_user(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.__check_attest_reg_resp(APP_ID, registration['keyHandle'],
                                 registration['pubKey'], USER_SECRET_2,
                                 VENDOR_CMD_RESPONSE_NOT_ALLOWED)

  def __test_attest_wrong_app_id(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.__check_attest_reg_resp(APP_ID_2, registration['keyHandle'],
                                 registration['pubKey'], USER_SECRET_1,
                                 VENDOR_CMD_RESPONSE_NOT_ALLOWED)

  def __test_attest_wrong_pub_key(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.__check_attest_reg_resp(APP_ID, registration['keyHandle'],
                                 'FF' * 65, USER_SECRET_1,
                                 VENDOR_CMD_RESPONSE_NOT_ALLOWED)

  def __test_attest_garbage_data(self):
    self.__u2f_attest(USER_SECRET_1, U2F_ATTEST_FORMAT_REG_RESP,
                      'ff' * U2F_ATTEST_REG_RESP_SIZE_BYTES,
                      VENDOR_CMD_RESPONSE_NOT_ALLOWED)

  def __test_attest_truncated_data(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    register_resp = '00{}{}{}{}'.format(
        APP_ID,
        RANDOM_32,  # challenge
        registration['keyHandle'],
        registration['pubKey'])

    # Attempt to attest to valid data with invalid format.
    self.__u2f_attest(USER_SECRET_1, U2F_ATTEST_FORMAT_REG_RESP, register_resp,
                      VENDOR_CMD_RESPONSE_BOGUS_ARGS, truncated=True)

  def __test_attest_invalid_format(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    register_resp = '00{}{}{}{}'.format(
        APP_ID,
        RANDOM_32,  # challenge
        registration['keyHandle'],
        registration['pubKey'])

    # Attempt to attest to valid data with invalid format.
    self.__u2f_attest(USER_SECRET_1, 'ff', register_resp,
                      VENDOR_CMD_RESPONSE_NOT_ALLOWED)

  def __test_attest_invalid_reserved_byte(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    register_resp = '{}{}{}{}{}'.format(
        '01', # unexpected reserved byte
        APP_ID,
        RANDOM_32,  # challenge
        registration['keyHandle'],
        registration['pubKey'])

    # Attempt to attest to valid data with invalid format.
    self.__u2f_attest(USER_SECRET_1, U2F_ATTEST_FORMAT_REG_RESP, register_resp,
                      VENDOR_CMD_RESPONSE_NOT_ALLOWED)

  def __test_kh_invalidated_by_powerwash(self):
    registration = self.__u2f_generate(APP_ID, USER_SECRET_1, '00')

    self.servo.power_short_press()

    # Sanity check
    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_SUCCESS)

    # Clear TPM. We should no longer be able to authenticate with the
    # key handle after this.
    tpm_utils.ClearTPMOwnerRequest(self.client, wait_for_ready=True)

    self.servo.power_short_press()

    self.__u2f_sign(APP_ID, USER_SECRET_1, registration['keyHandle'],
                    HASH_TO_SIGN, '00', VENDOR_CMD_RESPONSE_PASSWORD_REQUIRED)

  def run_once(self, host=None):
    """Run the tests."""

    self.client = host
    self.servo = host.servo
    self.servo.initialize_dut()

    # Basic functionality
    self.__test_generate_unique()
    self.__test_generate_sign_simple()

    # Generate - presence
    self.__test_generate_with_presence()
    self.__test_generate_consume_presence()

    # Sign - presence
    self.__test_sign_requires_presence()
    self.__test_sign_multiple_no_consume()
    self.__test_sign_consume()

    # Sign - key handle
    self.__test_sign_wrong_user_secret()
    self.__test_sign_wrong_app_id()
    self.__test_sign_invalid_kh()

    # Sign - check only
    self.__test_sign_check_only()
    self.__test_sign_check_only_with_presence()
    self.__test_sign_check_only_invalid_kh()
    self.__test_sign_check_only_invalid_kh_with_presence()

    # Attest
    self.__test_attest_simple()
    self.__test_attest_simple_padded()
    self.__test_attest_wrong_user()
    self.__test_attest_wrong_app_id()
    self.__test_attest_wrong_pub_key()
    self.__test_attest_garbage_data()
    self.__test_attest_truncated_data()
    self.__test_attest_invalid_format()
    self.__test_attest_invalid_reserved_byte()

    # Powerwash
    self.__test_kh_invalidated_by_powerwash()
