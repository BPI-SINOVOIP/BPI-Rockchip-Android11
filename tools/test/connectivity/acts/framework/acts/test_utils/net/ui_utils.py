"""Utils for adb-based UI operations."""

import collections
import logging
import os
import re
import time

from xml.dom import minidom
from acts.controllers.android_lib.errors import AndroidDeviceError


class Point(collections.namedtuple('Point', ['x', 'y'])):

  def __repr__(self):
    return '{x},{y}'.format(x=self.x, y=self.y)


class Bounds(collections.namedtuple('Bounds', ['start', 'end'])):

  def __repr__(self):
    return '[{start}][{end}]'.format(start=str(self.start), end=str(self.end))

  def calculate_middle_point(self):
    return Point((self.start.x + self.end.x) // 2,
                 (self.start.y + self.end.y) // 2)


def get_key_value_pair_strings(kv_pairs):
  return ' '.join(['%s="%s"' % (k, v) for k, v in kv_pairs.items()])


def parse_bound(bounds_string):
  """Parse UI bound string.

  Args:
    bounds_string: string, In the format of the UI element bound.
                   e.g '[0,0][1080,2160]'

  Returns:
    Bounds, The bound of UI element.
  """
  bounds_pattern = re.compile(r'\[(\d+),(\d+)\]\[(\d+),(\d+)\]')
  points = bounds_pattern.match(bounds_string).groups()
  points = list(map(int, points))
  return Bounds(Point(*points[:2]), Point(*points[-2:]))


def _find_point_in_bounds(bounds_string):
  """Finds a point that resides within the given bounds.

  Args:
    bounds_string: string, In the format of the UI element bound.

  Returns:
    A tuple of integers, representing X and Y coordinates of a point within
    the given boundary.
  """
  return parse_bound(bounds_string).calculate_middle_point()


def get_screen_dump_xml(device):
  """Gets an XML dump of the current device screen.

  This only works when there is no instrumentation process running. A running
  instrumentation process will disrupt calls for `adb shell uiautomator dump`.

  Args:
    device: AndroidDevice object.

  Returns:
    XML Document of the screen dump.
  """
  os.makedirs(device.log_path, exist_ok=True)
  device.adb.shell('uiautomator dump')
  device.adb.pull('/sdcard/window_dump.xml %s' % device.log_path)
  return minidom.parse('%s/window_dump.xml' % device.log_path)


def match_node(node, **matcher):
  """Determine if a mode matches with the given matcher.

  Args:
    node: Is a XML node to be checked against matcher.
    **matcher: Is a dict representing mobly AdbUiDevice matchers.

  Returns:
    True if all matchers match the given node.
  """
  match_list = []
  for k, v in matcher.items():
    if k == 'class_name':
      key = k.replace('class_name', 'class')
    elif k == 'text_contains':
      key = k.replace('text_contains', 'text')
    else:
      key = k.replace('_', '-')
    try:
      if k == 'text_contains':
        match_list.append(v in node.attributes[key].value)
      else:
        match_list.append(node.attributes[key].value == v)
    except KeyError:
      match_list.append(False)
  return all(match_list)


def _find_node(screen_dump_xml, **kwargs):
  """Finds an XML node from an XML DOM.

  Args:
    screen_dump_xml: XML doc, parsed from adb ui automator dump.
    **kwargs: key/value pairs to match in an XML node's attributes. Value of
      each key has to be string type. Below lists keys which can be used:
        index
        text
        text_contains (matching a part of text attribute)
        resource_id
        class_name (representing "class" attribute)
        package
        content_desc
        checkable
        checked
        clickable
        enabled
        focusable
        focused
        scrollable
        long_clickable
        password
        selected

  Returns:
    XML node of the UI element or None if not found.
  """
  nodes = screen_dump_xml.getElementsByTagName('node')
  for node in nodes:
    if match_node(node, **kwargs):
      logging.debug('Found a node matching conditions: %s',
                    get_key_value_pair_strings(kwargs))
      return node


def wait_and_get_xml_node(device, timeout, child=None, sibling=None, **kwargs):
  """Waits for a node to appear and return it.

  Args:
    device: AndroidDevice object.
    timeout: float, The number of seconds to wait for before giving up.
    child: dict, a dict contains child XML node's attributes. It is extra set of
      conditions to match an XML node that is under the XML node which is found
      by **kwargs.
    sibling: dict, a dict contains sibling XML node's attributes. It is extra
      set of conditions to match an XML node that is under parent of the XML
      node which is found by **kwargs.
    **kwargs: Key/value pairs to match in an XML node's attributes.

  Returns:
    The XML node of the UI element.

  Raises:
    AndroidDeviceError: if the UI element does not appear on screen within
    timeout or extra sets of conditions of child and sibling are used in a call.
  """
  if child and sibling:
    raise AndroidDeviceError(
        device, 'Only use one extra set of conditions: child or sibling.')
  start_time = time.time()
  threshold = start_time + timeout
  while time.time() < threshold:
    time.sleep(1)
    screen_dump_xml = get_screen_dump_xml(device)
    node = _find_node(screen_dump_xml, **kwargs)
    if node and child:
      node = _find_node(node, **child)
    if node and sibling:
      node = _find_node(node.parentNode, **sibling)
    if node:
      return node
  msg = ('Timed out after %ds waiting for UI node matching conditions: %s.'
         % (timeout, get_key_value_pair_strings(kwargs)))
  if child:
    msg = ('%s extra conditions: %s'
           % (msg, get_key_value_pair_strings(child)))
  if sibling:
    msg = ('%s extra conditions: %s'
           % (msg, get_key_value_pair_strings(sibling)))
  raise AndroidDeviceError(device, msg)


def has_element(device, **kwargs):
  """Checks a UI element whether appears or not in the current screen.

  Args:
    device: AndroidDevice object.
    **kwargs: Key/value pairs to match in an XML node's attributes.

  Returns:
    True if the UI element appears in the current screen else False.
  """
  timeout_sec = kwargs.pop('timeout', 30)
  try:
    wait_and_get_xml_node(device, timeout_sec, **kwargs)
    return True
  except AndroidDeviceError:
    return False


def get_element_attributes(device, **kwargs):
  """Gets a UI element's all attributes.

  Args:
    device: AndroidDevice object.
    **kwargs: Key/value pairs to match in an XML node's attributes.

  Returns:
    XML Node Attributes.
  """
  timeout_sec = kwargs.pop('timeout', 30)
  node = wait_and_get_xml_node(device, timeout_sec, **kwargs)
  return node.attributes


def wait_and_click(device, duration_ms=None, **kwargs):
  """Wait for a UI element to appear and click on it.

  This function locates a UI element on the screen by matching attributes of
  nodes in XML DOM, calculates a point's coordinates within the boundary of the
  element, and clicks on the point marked by the coordinates.

  Args:
    device: AndroidDevice object.
    duration_ms: int, The number of milliseconds to long-click.
    **kwargs: A set of `key=value` parameters that identifies a UI element.
  """
  timeout_sec = kwargs.pop('timeout', 30)
  button_node = wait_and_get_xml_node(device, timeout_sec, **kwargs)
  x, y = _find_point_in_bounds(button_node.attributes['bounds'].value)
  args = []
  if duration_ms is None:
    args = 'input tap %s %s' % (str(x), str(y))
  else:
    # Long click.
    args = 'input swipe %s %s %s %s %s' % \
        (str(x), str(y), str(x), str(y), str(duration_ms))
  device.adb.shell(args)

def wait_and_input_text(device, input_text, duration_ms=None, **kwargs):
  """Wait for a UI element text field that can accept text entry.

  This function located a UI element using wait_and_click. Once the element is
  clicked, the text is input into the text field.

  Args:
    device: AndroidDevice, Mobly's Android controller object.
    input_text: Text string to be entered in to the text field.
    duration_ms: duration in milliseconds.
    **kwargs: A set of `key=value` parameters that identifies a UI element.
  """
  wait_and_click(device, duration_ms, **kwargs)
  # Replace special characters.
  # The command "input text <string>" requires special treatment for
  # characters ' ' and '&'.  They need to be escaped. for example:
  #    "hello world!!&" needs to transform to "hello\ world!!\&"
  special_chars = ' &'
  for c in special_chars:
    input_text = input_text.replace(c, '\\%s' % c)
  input_text = "'" + input_text + "'"
  args = 'input text %s' % input_text
  device.adb.shell(args)
