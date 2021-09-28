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

r"""Read a MultiCarrierSettings file and update CarrierSettings data.

For APNs in the input file, they are simply appended to the apn list of the
corresponding carrier in CarrierSettings data. If a new carrier (identified by
canonical_name) appears in input, the other_carriers.textpb will be updated.

How to run:

update_carrier_data.par \
--in_file=/tmp/tmpapns.textpb \
--data_dir=/tmp/carrier/data
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
import argparse
import copy
import os
import compare
from google.protobuf import text_format
import carrier_list_pb2
import carrier_settings_pb2

parser = argparse.ArgumentParser()
parser.add_argument(
    '--data_dir', default='./data', help='Folder path for CarrierSettings data')
parser.add_argument(
    '--in_file', default='./tmpapns.textpb', help='Temp APN file')
FLAGS = parser.parse_args()

TIER1_CARRIERS_TEXTPB = os.path.join(FLAGS.data_dir, 'tier1_carriers.textpb')
OTHER_CARRIERS_TEXTPB = os.path.join(FLAGS.data_dir, 'other_carriers.textpb')


def equals_apn(a, b):
  """Tell if two ApnItem proto are the same."""
  a = compare.NormalizeRepeatedFields(copy.deepcopy(a))
  b = compare.NormalizeRepeatedFields(copy.deepcopy(b))
  # ignore 'name' field
  a.ClearField('name')
  b.ClearField('name')
  return compare.Proto2Equals(a, b)


def find_apn(apn, apns):
  """Tell if apn is in apns."""
  for a in apns:
    if equals_apn(apn, a):
      return True
  return False


def merge_mms_apn(a, b):
  """Try to merge mmsc fields of b into a, if that's the only diff."""
  aa = compare.NormalizeRepeatedFields(copy.deepcopy(a))
  bb = compare.NormalizeRepeatedFields(copy.deepcopy(b))
  # check if any fields other than mms are different
  for field in ['name', 'mmsc_proxy', 'mmsc_proxy_port']:
    aa.ClearField(field)
    bb.ClearField(field)
  if compare.Proto2Equals(aa, bb):
    for field in ['mmsc_proxy', 'mmsc_proxy_port']:
      if b.HasField(field):
        setattr(a, field, getattr(b, field))


def clean_apn(setting):
  """Remove duplicated ApnItems from a CarrierSettings proto.

  Args:
    setting: a CarrierSettings proto

  Returns:
    None
  """
  if not setting.HasField('apns') or len(setting.apns.apn) <= 1:
    return
  apns = setting.apns.apn[:]
  cleaned_apns = [a for n, a in enumerate(apns) if not find_apn(a, apns[:n])]
  del setting.apns.apn[:]
  setting.apns.apn.extend(cleaned_apns)


def merge_apns(dest_apns, source_apns):
  """Merge source_apns into dest_apns."""
  for apn in dest_apns:
    for source in source_apns:
      merge_mms_apn(apn, source)
  ret = list(dest_apns)
  for source in source_apns:
    if not find_apn(source, ret):
      ret.append(source)
  return ret


def to_string(cid):
  """Return the string representation of a CarrierId."""
  ret = cid.mcc_mnc
  if cid.HasField('spn'):
    ret += 'SPN=' + cid.spn.upper()
  if cid.HasField('imsi'):
    ret += 'IMSI=' + cid.imsi.upper()
  if cid.HasField('gid1'):
    ret += 'GID1=' + cid.gid1.upper()
  return ret


def to_carrier_id(cid_string):
  """Return the CarrierId from its string representation."""
  cid = carrier_list_pb2.CarrierId()
  if 'SPN=' in cid_string:
    ind = cid_string.find('SPN=')
    cid.mcc_mnc = cid_string[:ind]
    cid.spn = cid_string[ind + len('SPN='):]
  elif 'IMSI=' in cid_string:
    ind = cid_string.find('IMSI=')
    cid.mcc_mnc = cid_string[:ind]
    cid.imsi = cid_string[ind + len('IMSI='):]
  elif 'GID1=' in cid_string:
    ind = cid_string.find('GID1=')
    cid.mcc_mnc = cid_string[:ind]
    cid.gid1 = cid_string[ind + len('GID1='):]
  else:
    cid.mcc_mnc = cid_string
  return cid


def get_input(path):
  """Read input MultiCarrierSettings textpb file.

  Args:
    path: the path to input MultiCarrierSettings textpb file

  Returns:
    A MultiCarrierSettings. None when failed.
  """
  mcs = None
  with open(path, 'r', encoding='utf-8') as f:
    mcs = carrier_settings_pb2.MultiCarrierSettings()
    text_format.Merge(f.read(), mcs)

  return mcs


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
      carriers = carrier_list_pb2.CarrierList()
      text_format.Merge(f.read(), carriers)
      for carriermap in carriers.entry:
        for cid in carriermap.carrier_id:
          ret[to_string(cid)] = carriermap.canonical_name

  return ret


def clear_apn_fields_in_default_value(carrier_settings):

  def clean(apn):
    if apn.HasField('bearer_bitmask') and apn.bearer_bitmask == '0':
      apn.ClearField('bearer_bitmask')
    return apn

  for apn in carrier_settings.apns.apn:
    clean(apn)
  return carrier_settings


def merge_carrier_settings(patch, carrier_file):
  """Merge a CarrierSettings into a base CarrierSettings in textpb file.

  This function merge apns only. It assumes that the patch and base have the
  same canonical_name.

  Args:
    patch: the carrier_settings to be merged
    carrier_file: the path to the base carrier_settings file
  """
  # Load base
  with open(carrier_file, 'r', encoding='utf-8') as f:
    base_setting = text_format.ParseLines(f,
                                          carrier_settings_pb2.CarrierSettings())

  clean_apn(patch)
  clean_apn(base_setting)

  # Merge apns
  apns = base_setting.apns.apn[:]
  apns = merge_apns(apns, patch.apns.apn[:])
  del base_setting.apns.apn[:]
  base_setting.apns.apn.extend(apns)

  # Write back
  with open(carrier_file, 'w', encoding='utf-8') as f:
    text_format.PrintMessage(base_setting, f, as_utf8=True)


def merge_multi_carrier_settings(patch_list, carrier_file):
  """Merge CarrierSettings into a base MultiCarrierSettings in textpb file.

  This function merge apns only. The base may or may not contains an entry with
  the same canonical_name as the patch.

  Args:
    patch_list: a list of CarrierSettings to be merged
    carrier_file: the path to the base MultiCarrierSettings file
  """
  # Load base
  with open(carrier_file, 'r', encoding='utf-8') as f:
    base_settings = text_format.ParseLines(
        f, carrier_settings_pb2.MultiCarrierSettings())

  for patch in patch_list:
    clean_apn(patch)
    # find the (first and only) entry with patch.canonical_name and update it.
    for setting in base_settings.setting:
      if setting.canonical_name == patch.canonical_name:
        clean_apn(setting)
        apns = setting.apns.apn[:]
        apns = merge_apns(apns, patch.apns.apn[:])
        del setting.apns.apn[:]
        setting.apns.apn.extend(apns)
        break
    # Or if no match, append it to base_settings
    else:
      base_settings.setting.extend([patch])

  # Write back
  with open(carrier_file, 'w', encoding='utf-8') as f:
    text_format.PrintMessage(base_settings, f, as_utf8=True)


def add_new_carriers(cnames, carrier_list_file):
  """Add a new carrier into a CarrierList in textpb file.

  The carrier_id of the new carrier is induced from the cname, assuming
  that the cname is constructed by to_string.

  Args:
    cnames: a list of canonical_name of new carriers
    carrier_list_file: the path to the CarrierList textpb file

  Returns:
    None
  """
  with open(carrier_list_file, 'r', encoding='utf-8') as f:
    carriers = text_format.ParseLines(f, carrier_list_pb2.CarrierList())

  for cname in cnames:
    # Append the new carrier
    new_carrier = carriers.entry.add()
    new_carrier.canonical_name = cname
    new_carrier.carrier_id.extend([to_carrier_id(cname)])

  tmp = sorted(carriers.entry, key=lambda c: c.canonical_name)
  del carriers.entry[:]
  carriers.entry.extend(tmp)

  with open(carrier_list_file, 'w', encoding='utf-8') as f:
    text_format.PrintMessage(carriers, f, as_utf8=True)


def add_apns_for_other_carriers_by_mccmnc(apns, tier1_carriers, other_carriers):
  """Add APNs for carriers in others.textpb that doesn't have APNs, by mccmnc.

  If a carrier defined as mccmnc + mvno_data doesn't hava APNs, it should use
  APNs from the carrier defined as mccmnc only.

  Modifies others.textpb file in-place.

  Args:
    apns: a list of CarrierSettings message with APNs only.
    tier1_carriers: parsed tier-1 carriers list; must not contain new carriers.
      A dict, key is to_string(carrier_id), value is cname.
    other_carriers: parsed other carriers list; must not contain new carriers. A
      dict, key is to_string(carrier_id), value is cname.
  """
  # Convert apns from a list to a map, key being the canonical_name
  apns_dict = {
      carrier_settings.canonical_name: carrier_settings
      for carrier_settings in apns
  }

  others_textpb = '%s/setting/others.textpb' % FLAGS.data_dir
  with open(others_textpb, 'r', encoding='utf-8') as others_textpb_file:
    others = text_format.ParseLines(others_textpb_file,
                                    carrier_settings_pb2.MultiCarrierSettings())

  for setting in others.setting:
    if not setting.HasField('apns'):
      carrier_id = to_carrier_id(setting.canonical_name)
      if carrier_id.HasField('mvno_data'):
        carrier_id.ClearField('mvno_data')
        carrier_id_str_of_mccmnc = to_string(carrier_id)
        cname_of_mccmnc = tier1_carriers.get(
            carrier_id_str_of_mccmnc) or other_carriers.get(
                carrier_id_str_of_mccmnc)
        if cname_of_mccmnc:
          apn = apns_dict.get(cname_of_mccmnc)
          if apn:
            setting.apns.CopyFrom(apn.apns)

  with open(others_textpb, 'w', encoding='utf-8') as others_textpb_file:
    text_format.PrintMessage(others, others_textpb_file, as_utf8=True)


def add_carrierconfig_for_new_carriers(cnames, tier1_carriers, other_carriers):
  """Add carrier configs for new (non-tier-1) carriers.

   For new carriers, ie. carriers existing in APN but not CarrierConfig:
      - for <mccmnc>: copy carrier config of <mcc>.
      - for <mccmnc>(GID1|SPN|IMSI)=<mvnodata>: copy carrier config of <mccmnc>,
      or <mcc>.

  Modifies others.textpb file in-place.

  Args:
    cnames: a list of canonical_name of new carriers.
    tier1_carriers: parsed tier-1 carriers list; must not contain new carriers.
      A dict, key is to_string(carrier_id), value is cname.
    other_carriers: parsed other carriers list; must not contain new carriers. A
      dict, key is to_string(carrier_id), value is cname.
  """
  carrier_configs_map = {}

  others_textpb = '%s/setting/others.textpb' % FLAGS.data_dir
  with open(others_textpb, 'r', encoding='utf-8') as others_textpb_file:
    others = text_format.ParseLines(others_textpb_file,
                                    carrier_settings_pb2.MultiCarrierSettings())
    for setting in others.setting:
      if setting.canonical_name in other_carriers:
        carrier_configs_map[setting.canonical_name] = setting.configs
  for cid_str, cname in tier1_carriers.items():
    tier1_textpb = '%s/setting/%s.textpb' % (FLAGS.data_dir, cname)
    with open(tier1_textpb, 'r', encoding='utf-8') as tier1_textpb_file:
      tier1 = text_format.ParseLines(tier1_textpb_file,
                                     carrier_settings_pb2.CarrierSettings())
      carrier_configs_map[cid_str] = tier1.configs

  for setting in others.setting:
    if setting.canonical_name in cnames:
      carrier_id = to_carrier_id(setting.canonical_name)
      mccmnc = carrier_id.mcc_mnc
      mcc = mccmnc[:3]
      if mccmnc in carrier_configs_map:
        setting.configs.config.extend(carrier_configs_map[mccmnc].config[:])
      elif mcc in carrier_configs_map:
        setting.configs.config.extend(carrier_configs_map[mcc].config[:])

  with open(others_textpb, 'w', encoding='utf-8') as others_textpb_file:
    text_format.PrintMessage(others, others_textpb_file, as_utf8=True)


def cleanup_mcc_only_carriers():
  """Removes mcc-only carriers from other_carriers.textpb & others.textpb.

  Modifies other_carriers.textpb file & others.textpb file in-place.
  """
  mcc_only_carriers = set()

  with open(
      OTHER_CARRIERS_TEXTPB, 'r',
      encoding='utf-8') as other_carriers_textpb_file:
    other_carriers = text_format.ParseLines(other_carriers_textpb_file,
                                            carrier_list_pb2.CarrierList())

  other_carriers_entry_with_mccmnc = []
  for carrier in other_carriers.entry:
    for carrier_id in carrier.carrier_id:
      if len(carrier_id.mcc_mnc) == 3:
        mcc_only_carriers.add(carrier.canonical_name)
      else:
        other_carriers_entry_with_mccmnc.append(carrier)
  del other_carriers.entry[:]
  other_carriers.entry.extend(other_carriers_entry_with_mccmnc)

  # Finish early if no mcc_only_carriers; that means no file modification
  # required.
  if not mcc_only_carriers:
    return

  with open(
      OTHER_CARRIERS_TEXTPB, 'w',
      encoding='utf-8') as other_carriers_textpb_file:
    text_format.PrintMessage(
        other_carriers, other_carriers_textpb_file, as_utf8=True)

  others_textpb = os.path.join(FLAGS.data_dir, 'setting', 'others.textpb')
  with open(others_textpb, 'r', encoding='utf-8') as others_textpb_file:
    others = text_format.ParseLines(others_textpb_file,
                                    carrier_settings_pb2.MultiCarrierSettings())
  copy_others_setting = others.setting[:]
  del others.setting[:]
  others.setting.extend([
      setting for setting in copy_others_setting
      if setting.canonical_name not in mcc_only_carriers
  ])

  with open(others_textpb, 'w', encoding='utf-8') as others_textpb_file:
    text_format.PrintMessage(others, others_textpb_file, as_utf8=True)


def main():
  apns = get_input(FLAGS.in_file).setting
  tier1_carriers = get_knowncarriers([TIER1_CARRIERS_TEXTPB])
  other_carriers = get_knowncarriers([OTHER_CARRIERS_TEXTPB])
  new_carriers = []

  # Step 1a: merge APNs into CarrierConfigs by canonical name.
  # Also find out "new carriers" existing in APNs but not in CarrierConfigs.
  other_carriers_patch = []
  for carrier_settings in apns:
    carrier_settings = clear_apn_fields_in_default_value(carrier_settings)

    cname = carrier_settings.canonical_name
    if cname in tier1_carriers.values():
      merge_carrier_settings(carrier_settings,
                             '%s/setting/%s.textpb' % (FLAGS.data_dir, cname))
    else:
      other_carriers_patch.append(carrier_settings)
      if cname not in other_carriers.values():
        new_carriers.append(cname)

  merge_multi_carrier_settings(other_carriers_patch,
                               '%s/setting/others.textpb' % FLAGS.data_dir)

  # Step 1b: populate carrier configs for new carriers.
  add_carrierconfig_for_new_carriers(new_carriers, tier1_carriers,
                                     other_carriers)

  # Step 2: merge new carriers into non-tier1 carrier list.
  add_new_carriers(new_carriers, OTHER_CARRIERS_TEXTPB)
  # Update other_carriers map
  other_carriers = get_knowncarriers([OTHER_CARRIERS_TEXTPB])

  # Step 3: merge APNs into CarrierConfigs by mccmnc: for a carrier defined
  # as mccmnc + gid/spn/imsi, if it doesn't have any APNs, it should use APNs
  # from carrier defined as mccmnc only.
  # Only handle non-tier1 carriers, as tier1 carriers are assumed to be better
  # maintained and are already having APNs defined.
  add_apns_for_other_carriers_by_mccmnc(apns, tier1_carriers, other_carriers)

  # Step 4: clean up mcc-only carriers; they're used in step 3 but should not
  # be in final carrier settings to avoid confusing CarrierSettings app.
  cleanup_mcc_only_carriers()


if __name__ == '__main__':
  main()
