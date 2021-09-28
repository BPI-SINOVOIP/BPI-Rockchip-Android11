#
# Copyright 2017 - The Android Open Source Project
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

_PERMISSION_GROUPS = 3  # 3 permission groups: owner, group, all users
_READ_PERMISSION = 4
_WRITE_PERMISSION = 2
_EXECUTE_PERMISSION = 1


def _HasPermission(permission_bits, groupIndex, permission):
    """Determines if the permission bits grant a permission to a group.

    Args:
        permission_bits: string, the octal permissions string (e.g. 741)
        groupIndex: int, the index of the group into the permissions string.
                    (e.g. 0 is owner group). If set to -1, then all groups are
                    checked.
        permission: the value of the permission.

    Returns:
        True if the group(s) has read permission.

    Raises:
        ValueError if the group or permission bits are invalid
    """
    if groupIndex >= _PERMISSION_GROUPS:
        raise ValueError("Invalid group: %s" % str(groupIndex))

    if len(permission_bits) != _PERMISSION_GROUPS:
        raise ValueError("Invalid permission bits: %s" % str(permission_bits))

    # Define the start/end group index
    start = groupIndex
    end = groupIndex + 1
    if groupIndex < 0:
        start = 0
        end = _PERMISSION_GROUPS

    for i in range(start, end):
        perm = int(permission_bits[i])  # throws ValueError if not an integer
        if perm > 7:
            raise ValueError("Invalid permission bit: %s" % str(perm))
        if perm & permission == 0:
            # Return false if any group lacks the permission
            return False
    # Return true if no group lacks the permission
    return True


def IsReadable(permission_bits):
    """Determines if the permission bits grant read permission to any group.

    Args:
        permission_bits: string, the octal permissions string (e.g. 741)

    Returns:
        True if any group has read permission.

    Raises:
        ValueError if the group or permission bits are invalid
    """
    return any([
        _HasPermission(permission_bits, i, _READ_PERMISSION)
        for i in range(_PERMISSION_GROUPS)
    ])


def IsWritable(permission_bits):
    """Determines if the permission bits grant write permission to any group.

    Args:
        permission_bits: string, the octal permissions string (e.g. 741)

    Returns:
        True if any group has write permission.

    Raises:
        ValueError if the group or permission bits are invalid
    """
    return any([
        _HasPermission(permission_bits, i, _WRITE_PERMISSION)
        for i in range(_PERMISSION_GROUPS)
    ])


def IsExecutable(permission_bits):
    """Determines if the permission bits grant execute permission to any group.

    Args:
        permission_bits: string, the octal permissions string (e.g. 741)

    Returns:
        True if any group has execute permission.

    Raises:
        ValueError if the group or permission bits are invalid
    """
    return any([
        _HasPermission(permission_bits, i, _EXECUTE_PERMISSION)
        for i in range(_PERMISSION_GROUPS)
    ])


def IsReadOnly(permission_bits):
    """Determines if the permission bits grant read-only permission.

    Read-only permission is granted if some group has read access but no group
    has write access.

    Args:
        permission_bits: string, the octal permissions string (e.g. 741)

    Returns:
        True if any group has read permission, none have write.

    Raises:
        ValueError if the group or permission bits are invalid
    """
    return IsReadable(permission_bits) and not IsWritable(permission_bits)


def IsWriteOnly(permission_bits):
    """Determines if the permission bits grant write-only permission.

    Write-only permission is granted if some group has write access but no group
    has read access.

    Args:
        permission_bits: string, the octal permissions string (e.g. 741)

    Returns:
        True if any group has write permission, none have read.

    Raises:
        ValueError if the group or permission bits are invalid
    """
    return IsWritable(permission_bits) and not IsReadable(permission_bits)


def IsReadWrite(permission_bits):
    """Determines if the permission bits grant read/write permissions.

    Read-write permission is granted if some group has read access and some has
    write access. The groups may be different.

    Args:
        permission_bits: string, the octal permissions string (e.g. 741)

    Returns:
        True if read and write permissions are granted to any group(s).

    Raises:
        ValueError if the group or permission bits are invalid
    """
    return IsReadable(permission_bits) and IsWritable(permission_bits)
