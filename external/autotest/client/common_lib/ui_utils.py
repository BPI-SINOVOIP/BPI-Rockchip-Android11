import logging
import time
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils


class UI_Handler(object):

    REGEX_ALL = '/(.*?)/'

    PROMISE_TEMPLATE = \
        '''new Promise(function(resolve, reject) {
            chrome.automation.getDesktop(function(root) {
                    resolve(%s);
                })
            })'''

    def start_ui_root(self, cr):
        """Start the UI root object for testing."""
        self.ext = cr.autotest_ext

    def is_obj_restricted(self, name, isRegex=False, role=None):
        """
        Return True if the object restriction is 'disabled'.
        This usually means the button is either greyed out, locked, etc.

        @param name: Parameter to provide to the 'name' attribute.
        @param isRegex: bool, if the item is a regex.
        @param role: Parameter to provide to the 'role' attribute.

        """
        FindParams = self._get_FindParams_str(name=name,
                                              role=role,
                                              isRegex=isRegex)
        try:
            restriction = self.ext.EvaluateJavaScript(
                self.PROMISE_TEMPLATE % ("%s.restriction" % FindParams),
                promise=True)
        except Exception:
            raise error.TestError(
                'Could not find object {}.'.format(name))
        if restriction == 'disabled':
            return True
        return False

    def item_present(self, name, isRegex=False, flip=False, role=None):
        """
        Determines if an object is present on the screen

        @param name: Parameter to provide to the 'name' attribute.
        @param isRegex: bool, if the 'name' is a regex.
        @param flip: Flips the return status.
        @param role: Parameter to provide to the 'role' attribute.

        @returns:
            True if object is present and flip is False.
            False if object is present and flip is True.
            False if object is not present and flip is False.
            True if object is not present and flip is True.

        """
        FindParams = self._get_FindParams_str(name=name,
                                              role=role,
                                              isRegex=isRegex)

        if flip is True:
            return not self._is_item_present(FindParams)
        return self._is_item_present(FindParams)

    def wait_for_ui_obj(self,
                        name,
                        isRegex=False,
                        remove=False,
                        role=None,
                        timeout=10):
        """
        Waits for the UI object specified.

        @param name: Parameter to provide to the 'name' attribute.
        @param isRegex: bool, if the 'name' is a regex.
        @param remove: bool, if you are waiting for the item to be removed.
        @param role: Parameter to provide to the 'role' attribute.
        @param timeout: int, time to wait for the item.

        @raises error.TestError if the element is not loaded (or removed).

        """
        utils.poll_for_condition(
            condition=lambda: self.item_present(name=name,
                                                isRegex=isRegex,
                                                flip=remove,
                                                role=role),
            timeout=timeout,
            exception=error.TestError('{} did not load in: {}'
                                      .format(name, self.list_screen_items())))

    def did_obj_not_load(self, name, isRegex=False, timeout=5):
        """
        Specifically used to wait and see if an item appears on the UI.

        NOTE: This is different from wait_for_ui_obj because that returns as
        soon as the object is either loaded or not loaded. This function will
        wait to ensure over the timeout period the object never loads.
        Additionally it will return as soon as it does load. Basically a fancy
        time.sleep()

        @param name: Parameter to provide to the 'name' attribute.
        @param isRegex: bool, if the item is a regex.
        @param timeout: Time in seconds to wait for the object to appear.

        @returns: True if object never loaded within the timeout period,
            else False.

        """
        t1 = time.time()
        while time.time() - t1 < timeout:
            if self.item_present(name=name, isRegex=isRegex):
                return False
            time.sleep(1)
        return True

    def doDefault_on_obj(self, name, isRegex=False, role=None):
        """Runs the .doDefault() js command on the element."""
        FindParams = self._get_FindParams_str(name=name,
                                              role=role,
                                              isRegex=isRegex)
        try:
            self.ext.EvaluateJavaScript(
                self.PROMISE_TEMPLATE % ("%s.doDefault()" % FindParams),
                promise=True)
        except:
            logging.info('Unable to .doDefault() on {}. All items: {}'
                         .format(FindParams, self.list_screen_items()))
            raise error.TestError("doDefault failed on {}".format(FindParams))

    def doCommand_on_obj(self, name, cmd, isRegex=False, role=None):
        """Run the specified command on the element."""
        FindParams = self._get_FindParams_str(name=name,
                                              role=role,
                                              isRegex=isRegex)
        return self.ext.EvaluateJavaScript(self.PROMISE_TEMPLATE % """
            %s.%s""" % (FindParams, cmd), promise=True)

    def list_screen_items(self,
                          role=None,
                          name=None,
                          isRegex=False,
                          attr='name'):

        """
        List all the items currently visable on the screen.

        If no paramters are given, it will return the name of each item,
        including items with empty names.

        @param role: The role of the items to use (ie button).
        @param name: Parameter to provide to the 'name' attribute.
        @param isRegex: bool, if the obj is a regex.
        @param attr: Str, the attribute you want returned in the list
            (eg 'name').

        """

        if isRegex:
            if name is None:
                raise error.TestError('If regex is True name must be given')
            name = self._format_obj(name, isRegex)
        elif name is not None:
            name = self._format_obj(name, isRegex)
        name = self.REGEX_ALL if name is None else name
        role = self.REGEX_ALL if role is None else self._format_obj(role,
                                                                    False)

        new_promise = self.PROMISE_TEMPLATE % """root.findAll({attributes:
            {name: %s, role: %s}}).map(node => node.%s)""" % (name, role, attr)

        return self.ext.EvaluateJavaScript(new_promise, promise=True)

    def get_name_role_list(self):
        """
        Return [{}, {}] containing the name/role of everything on screen.

        """
        combined = []
        names = self.list_screen_items(attr='name')
        roles = self.list_screen_items(attr='role')

        if len(names) != len(roles):
            raise error.TestError('Number of items in names and roles !=')

        for name, role in zip(names, roles):
            temp_d = {'name': name, 'role': role}
            combined.append(temp_d)
        return combined

    def _format_obj(self, name, isRegex):
        """
        Formats the object for use in the javascript name attribute.

        When searching for an element on the UI, a regex expression or string
        can be used. If the search is using a string, the obj will need to be
        wrapped in quotes. A Regex is not.

        @param name: Parameter to provide to the 'name' attribute.
        @param isRegex: if True, the object will be returned as is, if False
            the obj will be returned wrapped in quotes.

        @returns: The formatted string for regex/name.
        """
        if isRegex:
            return name
        else:
            return '"{}"'.format(name)

    def _get_FindParams_str(self, name, role, isRegex):
        """Returns the FindParms string, so that automation node functions
        can be run on it

        @param role: The role of the items to use (ie button).
        @param name: Parameter to provide to the 'name' attribute.
        @param isRegex: bool, if the obj is a regex.

        @returns: The ".find($FindParams)" string, which can be used to run
            automation node commands, such as .doDefault()

        """
        FINDPARAMS_BASE = """
        root.find({attributes:
                  {name: %s,
                   role: %s}}
                 )"""

        name = self._format_obj(name, isRegex)
        if role is None:
            role = self.REGEX_ALL
        else:
            role = self._format_obj(role, False)
        return (FINDPARAMS_BASE % (name, role))

    def _is_item_present(self, findParams):
        """Return False if tempVar is None, else True."""
        item_present = self.ext.EvaluateJavaScript(
            self.PROMISE_TEMPLATE % findParams,
            promise=True)
        if item_present is None:
            return False
        return True

    def click_and_wait_for_item_with_retries(self,
                                             item_to_click,
                                             item_to_wait_for,
                                             isRegex_click=False,
                                             isRegex_wait=False,
                                             click_role=None,
                                             wait_role=None):
        """
        Click on an item, and wait for a subsequent item to load. If the new
        item does not load, we attempt to click the button again.

        This being done to remove the corner case of button being visually
        loaded, but not fully ready to be clicked yet. In simple terms:
            Click button --> Check for next button to appear
            IF button does not appear, its likely the original button click did
            not work, thus reclick that button --> recheck for the next button.

            If button did appear, stop clicking.

        """
        self.doDefault_on_obj(item_to_click,
                              role=click_role,
                              isRegex=isRegex_click)
        for retry in xrange(3):
            try:
                self.wait_for_ui_obj(item_to_wait_for,
                                     role=wait_role,
                                     isRegex=isRegex_wait,
                                     timeout=6)
                break
            except error.TestError:
                self.doDefault_on_obj(item_to_click,
                                      role=click_role,
                                      isRegex=isRegex_click)
        else:
            raise error.TestError('Item {} did not load after 2 tries'.format(
                                  item_to_wait_for))
