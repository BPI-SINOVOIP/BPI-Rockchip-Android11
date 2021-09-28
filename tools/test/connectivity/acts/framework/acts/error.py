"""This class is where error information will be stored.
"""

import json


class ActsError(Exception):
    """Base Acts Error"""
    def __init__(self, *args, **kwargs):
        class_name = self.__class__.__name__
        self.message = self.__class__.__doc__
        self.error_code = getattr(ActsErrorCode, class_name)
        self.extra = kwargs
        if len(args) > 0:
            self.extra['details'] = args

    def json_str(self):
        """Converts this error to a string in json format.

        Format of the json string is:
            {
                "ErrorCode": int
                "Message": str
                "Extra": any
            }

        Returns:
            A json-format string representing the errors
        """
        d = {}
        d['ErrorCode'] = self.error_code
        d['Message'] = self.message
        d['Extras'] = self.extra
        json_str = json.dumps(d, indent=4, sort_keys=True)
        return json_str


class ActsErrorCode:
    # Framework Errors 0-999

    # This error code is used to implement unittests for this class.
    ActsError = 100
    AndroidDeviceError = 101

    # Controllers Errors 1000-3999

    Sl4aStartError = 1001
    Sl4aApiError = 1002
    Sl4aConnectionError = 1003
    Sl4aProtocolError = 1004
    Sl4aNotInstalledError = 1005
    Sl4aRpcTimeoutError = 1006

    # Util Errors 4000-9999

    FastbootError = 9000
    AdbError = 9001
