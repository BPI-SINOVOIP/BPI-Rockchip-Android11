# Copyright 2019 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""The dom_util module.

This module is a collection of the help functions to operate with the minidom
relevant objects.
"""

# The temporary pylint disable statements are only used for the skeleton upload.
# pylint: disable=unused-argument
# pylint: disable=unnecessary-pass


def find_special_node(parent, element_name, attributes=None):
    """Finds the node contains the specific element.

    Find the node of the element contains the tag of element_name and the
    attribute values listed in the attributes dict. There're two cases as
    follows,
        1. The case with attributes is as below <component name="...">.
        2. The case without attributes is as below <findStrings>.
            <component name="FindInProjectRecents">
              <findStrings>
                <find>StackInfo</find>
                <find>attachStartupAgents</find>
              </findStrings>
            </component>

    Args:
        parent: A dom node object hierarchically contains the target_name
                element object.
        element_name: A string of the specific element name.
        attributes: A dict of {'name': 'value', ...} for the attributes used to
                    match the element.

    Returns:
        A node object if the element_name target is found, otherwise None.
    """
    pass


def compare_element_with_attributes(element, attributes=None):
    """Compares whether the element contains the multiple attributes.

    Args:
        element: A dom element object which is used to compare with.
        attributes: A dict of {'name': 'value', ...} for the attributes used to
                    match the element.

    Returns:
        boolean: True if the input element contains attributes the same with
                   the attributes, otherwise False.
    """
    pass


def update_element_attributes(node, element_name, attribute_set):
    """Updates attribute values into the matched element in the node.

    Use the element_name and the dict in attribute_set to find the first
    matched element and update its value with the last two items in
    attribute_set which are the name and value.

    Example:
        The following section demonstrates how to use this method to update
        the PORT value to 3000.

            <component name="RunManager">
              <configuration name="aidegen_jvm" type="Remote">
                <module name="remote_template_test" />
                <option name="HOST" value="localhost" />
                <option name="PORT" value="5005" />
              </configuration>
            </component>

    Args:
        node: The minidom node object contains the child element to be updated.
              In the example, it represents the RunManager component node.

        element_name: the string of the element tag, which is the 'option' in
                      the example.

        attribute_set: A set with 3 parts, ({'name': 'value', ,..}, name, value)

            {'name': 'value', ,..}: A dict of {'name': 'value', ...} for the
                                    attributes used to match the element.
            name: A string of the attribute name to update.
            value: An object with integer or string type used to update to the
                   name attribute.

            In the example, the ({'name': 'PORT'}, 'value', 3000) is the value
            of the attribute_set.

    Returns:
        True if update is successful.

    Raises:
        TypeError and AttributeError in bad input case.
    """
    pass


def update_element_with_condition(element, attributes, target_name,
                                  target_value):
    """Updates an element if it's fully matched with the compared condition.

    If all the attribute data of the element are the same as attributes,
    assign target_value to the target_name attribute in it.

    Args:
        element: The minidom element object.
        attributes: A dict of {'name 1': 'value 1', ..., 'name n': 'value n'}
                    for the attributes of the element.
        target_name: The string of the attribute name.
        target_value: An integer or string used to set value.

    Returns:
        boolean: False means there's no such element can be updated.

    Raises:
        TypeError and AttributeError in bad input case.
    """
    pass


def insert_element_data(element, attributes, target_name, target_value):
    """Inserts attribute data to an element.

    Args:
        element: The minidom element object.
        attributes: A dict of {'name 1': 'value 1', ..., 'name n': 'value n'}
                    for the attributes of the element.
        target_name: The string of the attribute name.
        target_value: An integer or string used to set value.

    Returns:
        True if update is successful.
    """
    pass
