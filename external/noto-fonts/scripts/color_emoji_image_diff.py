#!/usr/bin/env python

# Copyright (C) 2018 The Android Open Source Project
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

import contextlib
import itertools
import io
import os
import sys

from fontTools import ttLib
from PIL import Image

def to_hex_str(value):
    """Converts given int value to hex without the 0x prefix"""
    return format(value, 'X')

def codepoint_to_string(codepoints):
    """Converts a list of codepoints into a string separated with space."""
    return '_'.join([to_hex_str(x) for x in codepoints])

def read_cmap12(ttf, glyph_to_codepoint_map, codepoint_map):
    cmap = ttf['cmap']
    for table in cmap.tables:
        if table.format == 12 and table.platformID == 3 and table.platEncID == 10:
            for codepoint, glyph_name in table.cmap.iteritems():
                glyph_to_codepoint_map[glyph_name] = codepoint
                codepoint_map[codepoint_to_string([codepoint])] = glyph_name
                # self.update_emoji_data([codepoint], glyph_name)
            return table
    raise ValueError("Font doesn't contain cmap with format:12, platformID:3 and platEncID:10")

def read_gsub(ttf, glyph_to_codepoint_map, codepoint_map):
    gsub = ttf['GSUB']
    ligature_subtables = []
    context_subtables = []
    # this code is font dependent, implementing all gsub rules is out of scope of EmojiCompat
    # and would be expensive with little value
    for lookup in gsub.table.LookupList.Lookup:
        for subtable in lookup.SubTable:
            if subtable.LookupType == 5:
                context_subtables.append(subtable)
            elif subtable.LookupType == 4:
                ligature_subtables.append(subtable)

    for subtable in context_subtables:
        add_gsub_context_subtable(subtable, gsub.table.LookupList, glyph_to_codepoint_map, codepoint_map)

    for subtable in ligature_subtables:
        add_gsub_ligature_subtable(subtable, glyph_to_codepoint_map, codepoint_map)

def add_gsub_context_subtable(subtable, lookup_list, glyph_to_codepoint_map, codepoint_map):
    for sub_class_set in subtable.SubClassSet:
        if sub_class_set:
            for sub_class_rule in sub_class_set.SubClassRule:
                subs_list = len(sub_class_rule.SubstLookupRecord) * [None]
                for record in sub_class_rule.SubstLookupRecord:
                    subs_list[record.SequenceIndex] = get_substitutions(lookup_list,
                                                                        record.LookupListIndex)
                combinations = list(itertools.product(*subs_list))
                for seq in combinations:
                    glyph_names = [x["input"] for x in seq]
                    codepoints = [glyph_to_codepoint_map[x] for x in glyph_names]
                    outputs = [x["output"] for x in seq if x["output"]]
                    nonempty_outputs = filter(lambda x: x.strip() , outputs)
                    if len(nonempty_outputs) == 0:
                        print("Warning: no output glyph is set for " + str(glyph_names))
                        continue
                    elif len(nonempty_outputs) > 1:
                        print(
                            "Warning: multiple glyph is set for "
                                + str(glyph_names) + ", will use the first one")

                    glyph = nonempty_outputs[0]
                    codepoint_map[codepoint_to_string(codepoints)] = glyph

def get_substitutions(lookup_list, index):
    result = []
    for x in lookup_list.Lookup[index].SubTable:
        for input, output in x.mapping.iteritems():
            result.append({"input": input, "output": output})
    return result

def add_gsub_ligature_subtable(subtable, glyph_to_codepoint_map, codepoint_map):
    for name, ligatures in subtable.ligatures.iteritems():
        for ligature in ligatures:
            glyph_names = [name] + ligature.Component
            codepoints = [glyph_to_codepoint_map[x] for x in glyph_names]
            codepoint_map[codepoint_to_string(codepoints)] = ligature.LigGlyph

def read_cbdt(ttf):
  cbdt = ttf['CBDT']
  glyph_to_image = {}
  for strike_data in cbdt.strikeData:
    for key, data in strike_data.iteritems():
      data.decompile
      glyph_to_image[key] = Image.open(io.BytesIO(data.imageData))
  return glyph_to_image


rgba_map = {}

def similar_img(img1, img2):
    # return if images are the same with accepting some changes
    if img1 is None and img2 is None: return True
    if img1 is None or img2 is None: return False
    if not img1.size == img2.size: return False

    pixels1 = rgba_map.get(img1, img1.convert('L').getdata())
    pixels2 = rgba_map.get(img2, img2.convert('L').getdata())
    pixels = itertools.izip(pixels1, pixels2)
    diff = 0
    for px1, px2 in pixels:
        diff = diff + abs(px1-px2)
    pixel_count = 1.0 * img1.size[0] * img1.size[1]
    normalized_diff = diff / pixel_count / 255.0 * 100.0
    if normalized_diff <= 0.5: return True
    return False

def main(argv):
  codepoint_map_1 = {}
  codepoint_map_2 = {}
  glyph_to_codepoint_map_1 = {}
  glyph_to_codepoint_map_2 = {}

  with contextlib.closing(ttLib.TTFont(argv[1])) as ttf:
    font1_cbdt = read_cbdt(ttf)
    read_cmap12(ttf, glyph_to_codepoint_map_1, codepoint_map_1)
    read_gsub(ttf, glyph_to_codepoint_map_1, codepoint_map_1)
  with contextlib.closing(ttLib.TTFont(argv[2])) as ttf:
    font2_cbdt = read_cbdt(ttf)
    read_cmap12(ttf, glyph_to_codepoint_map_2, codepoint_map_2)
    read_gsub(ttf, glyph_to_codepoint_map_2, codepoint_map_2)

  glyphs1 = set(font1_cbdt.keys())
  glyphs2 = set(font2_cbdt.keys())

  codepoints_set1 = set(codepoint_map_1.keys())
  codepoints_set2 = set(codepoint_map_2.keys())

  if codepoints_set1 != codepoints_set2:
    print "Codepoints set has changed: : %s" % (codepoints_set1 ^ codepoints_set2)

  all_codepoints = set(codepoint_map_1.keys()).union(codepoint_map_2.keys())

  for key in all_codepoints:
    glyph1 = codepoint_map_1[key] if key in codepoint_map_1 else None
    glyph2 = codepoint_map_2[key] if key in codepoint_map_2 else None

    image1 = font1_cbdt[glyph1] if glyph1 and glyph1 in font1_cbdt else None
    image2 = font2_cbdt[glyph2] if glyph2 and glyph2 in font2_cbdt else None

    if not similar_img(image1, image2):
      print 'Glyph %s has different image' % key
      if image1:
        with open(os.path.join(argv[3], '%s_old.png' % key), 'w') as f:
            image1.save(f)
      if image2:
        with open(os.path.join(argv[3], '%s_new.png' % key), 'w') as f:
            image2.save(f)


def print_usage():
    """Prints how to use the script."""
    print("usage: old_font new_font output_dir")

if __name__ == '__main__':
   if len(sys.argv) < 3:
       print_usage()
       sys.exit(1)
   main(sys.argv)
