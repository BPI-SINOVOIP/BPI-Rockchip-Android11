# Copyright (C) 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

r"""Read APN conf xml file and output an textpb.

How to run:

update_apn.par --apn_file=./apns-full-conf.xml \
--data_dir=./data --out_file=/tmpapns.textpb
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
import argparse
import collections
from xml.dom import minidom
from google.protobuf import text_format

import carrier_list_pb2
import carrier_settings_pb2

parser = argparse.ArgumentParser()
parser.add_argument(
    '--apn_file', default='./apns-full-conf.xml', help='Path to APN xml file')
parser.add_argument(
    '--data_dir', default='./data', help='Folder path for CarrierSettings data')
parser.add_argument(
    '--out_file', default='./tmpapns.textpb', help='Temp APN file')
FLAGS = parser.parse_args()

CARRIER_LISTS = ['tier1_carriers.textpb', 'other_carriers.textpb']


def to_string(cid):
  """Return a string for CarrierId."""
  ret = cid.mcc_mnc
  if cid.HasField('spn'):
    ret += 'SPN=' + cid.spn.upper()
  if cid.HasField('imsi'):
    ret += 'IMSI=' + cid.imsi.upper()
  if cid.HasField('gid1'):
    ret += 'GID1=' + cid.gid1.upper()
  return ret


def get_cname(cid, known_carriers):
  """Return a canonical name based on cid and known_carriers.

  If found a match in known_carriers, return it. Otherwise generate a new one
  by concating the values.

  Args:
    cid: proto of CarrierId
    known_carriers: mapping from mccmnc and possible mvno data to canonical name

  Returns:
    string for canonical name, like verizon_us or 27402
  """
  name = to_string(cid)
  if name in known_carriers:
    return known_carriers[name]
  else:
    return name


def get_knowncarriers(files):
  """Create a mapping from mccmnc and possible mvno data to canonical name.

  Args:
    files: list of paths to carrier list textpb files

  Returns:
    A dict, key is to_string(carrier_id), value is cname.
  """
  ret = dict()
  for path in files:
    with open(path, 'r', encoding='utf-8') as f:
      carriers = text_format.Parse(f.read(), carrier_list_pb2.CarrierList())
      for carriermap in carriers.entry:
        # could print error if already exist
        for cid in carriermap.carrier_id:
          ret[to_string(cid)] = carriermap.canonical_name

  return ret


def gen_cid(apn_node):
  """Generate carrier id proto from APN node.

  Args:
    apn_node: DOM node from getElementsByTag

  Returns:
    CarrierId proto
  """
  ret = carrier_list_pb2.CarrierId()
  ret.mcc_mnc = (apn_node.getAttribute('mcc') + apn_node.getAttribute('mnc'))
  mvno_type = apn_node.getAttribute('mvno_type')
  mvno_data = apn_node.getAttribute('mvno_match_data')
  if mvno_type.lower() == 'spn':
    ret.spn = mvno_data
  if mvno_type.lower() == 'imsi':
    ret.imsi = mvno_data
  # in apn xml, gid means gid1, and no gid2
  if mvno_type.lower() == 'gid':
    ret.gid1 = mvno_data
  return ret


APN_TYPE_MAP = {
    '*': carrier_settings_pb2.ApnItem.ALL,
    'default': carrier_settings_pb2.ApnItem.DEFAULT,
    'internet': carrier_settings_pb2.ApnItem.DEFAULT,
    'vzw800': carrier_settings_pb2.ApnItem.DEFAULT,
    'mms': carrier_settings_pb2.ApnItem.MMS,
    'sup': carrier_settings_pb2.ApnItem.SUPL,
    'supl': carrier_settings_pb2.ApnItem.SUPL,
    'agps': carrier_settings_pb2.ApnItem.SUPL,
    'pam': carrier_settings_pb2.ApnItem.DUN,
    'dun': carrier_settings_pb2.ApnItem.DUN,
    'hipri': carrier_settings_pb2.ApnItem.HIPRI,
    'ota': carrier_settings_pb2.ApnItem.FOTA,
    'fota': carrier_settings_pb2.ApnItem.FOTA,
    'admin': carrier_settings_pb2.ApnItem.FOTA,
    'ims': carrier_settings_pb2.ApnItem.IMS,
    'cbs': carrier_settings_pb2.ApnItem.CBS,
    'ia': carrier_settings_pb2.ApnItem.IA,
    'emergency': carrier_settings_pb2.ApnItem.EMERGENCY,
    'xcap': carrier_settings_pb2.ApnItem.XCAP,
    'ut': carrier_settings_pb2.ApnItem.UT,
}


def map_apntype(typestr):
  """Map from APN type string to list of ApnType enums.

  Args:
    typestr: APN type string in apn conf xml, comma separated
  Returns:
    List of ApnType values in ApnItem proto.
  """
  typelist = [apn.strip().lower() for apn in typestr.split(',')]
  return list(set([APN_TYPE_MAP[t] for t in typelist if t]))


APN_PROTOCOL_MAP = {
    'ip': carrier_settings_pb2.ApnItem.IP,
    'ipv4': carrier_settings_pb2.ApnItem.IP,
    'ipv6': carrier_settings_pb2.ApnItem.IPV6,
    'ipv4v6': carrier_settings_pb2.ApnItem.IPV4V6,
    'ppp': carrier_settings_pb2.ApnItem.PPP
}

BOOL_MAP = {'true': True, 'false': False, '1': True, '0': False}

APN_SKIPXLAT_MAP = {
    -1: carrier_settings_pb2.ApnItem.SKIP_464XLAT_DEFAULT,
    0: carrier_settings_pb2.ApnItem.SKIP_464XLAT_DISABLE,
    1: carrier_settings_pb2.ApnItem.SKIP_464XLAT_ENABLE
}

# not include already handeld string keys like mcc, protocol
APN_STRING_KEYS = [
    'bearer_bitmask', 'server', 'proxy', 'port', 'user', 'password', 'mmsc',
    'mmsc_proxy', 'mmsc_proxy_port'
]

# keys that are different between apn.xml and apn.proto
APN_REMAP_KEYS = {
    'mmsproxy': 'mmsc_proxy',
    'mmsport': 'mmsc_proxy_port'
}

APN_INT_KEYS = [
    'authtype', 'mtu', 'profile_id', 'max_conns', 'wait_time', 'max_conns_time'
]

APN_BOOL_KEYS = [
    'modem_cognitive', 'user_visible', 'user_editable'
]


def gen_apnitem(node):
  """Create ApnItem proto based on APN node from xml file.

  Args:
    node: xml dom node from apn conf xml file.

  Returns:
    An ApnItem proto.
  """
  apn = carrier_settings_pb2.ApnItem()
  apn.name = node.getAttribute('carrier')
  apn.value = node.getAttribute('apn')
  apn.type.extend(map_apntype(node.getAttribute('type')))
  for key in ['protocol', 'roaming_protocol']:
    if node.hasAttribute(key):
      setattr(apn, key, APN_PROTOCOL_MAP[node.getAttribute(key).lower()])

  for key in node.attributes.keys():
    # Treat bearer as bearer_bitmask if no bearer_bitmask specified
    if key == 'bearer' and not node.hasAttribute('bearer_bitmask'):
      setattr(apn, 'bearer_bitmask', node.getAttribute(key))
      continue
    if key == 'skip_464xlat':
      setattr(apn, key, APN_SKIPXLAT_MAP[int(node.getAttribute(key))])
      continue
    if key in APN_STRING_KEYS:
      setattr(apn, key, node.getAttribute(key))
      continue
    if key in APN_REMAP_KEYS:
      setattr(apn, APN_REMAP_KEYS[key], node.getAttribute(key))
      continue
    if key in APN_INT_KEYS:
      setattr(apn, key, int(node.getAttribute(key)))
      continue
    if key in APN_BOOL_KEYS:
      setattr(apn, key, BOOL_MAP[node.getAttribute(key).lower()])
      continue

  return apn


def main():
  known = get_knowncarriers([FLAGS.data_dir + '/' + f for f in CARRIER_LISTS])

  with open(FLAGS.apn_file, 'r', encoding='utf-8') as apnfile:
    dom = minidom.parse(apnfile)

  apn_map = collections.defaultdict(list)
  for apn_node in dom.getElementsByTagName('apn'):
    cname = get_cname(gen_cid(apn_node), known)
    apn = gen_apnitem(apn_node)
    apn_map[cname].append(apn)

  mcs = carrier_settings_pb2.MultiCarrierSettings()
  for c in apn_map:
    carriersettings = mcs.setting.add()
    carriersettings.canonical_name = c
    carriersettings.apns.apn.extend(apn_map[c])

  with open(FLAGS.out_file, 'w', encoding='utf-8') as apnout:
    apnout.write(text_format.MessageToString(mcs, as_utf8=True))


if __name__ == '__main__':
  main()
