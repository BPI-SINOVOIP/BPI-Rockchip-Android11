#!/usr/bin/env python
import sys
import xml.etree.ElementTree as ET
import xml.dom.minidom as minidom
import os
import datetime
from optparse import OptionParser

def cal_crop(source, target):
    ratio_src = source['W'] * 1.0 / source['H']
    ratio_dst = target['W'] * 1.0 / target['H']

    ret = {}

    if ratio_src == ratio_dst:
        height = source['H']
        width = source['W']
    elif ratio_src > ratio_dst:
        height = source['H']
        width = int(height * ratio_dst)
    else:
        width = source['W']
        height = int(width / ratio_dst)
        height = height - height % 2

    return {'W': width, 'H': height}

def cal_reso(reso, sp_reso, mp_reso):
    ret = {}

    """ sp_reso always smaller than mp_reso"""
    if mp_reso:
        ret['sensor'] = reso[0] if len(reso) == 1 or mp_reso['W'] > reso[1]['W'] else reso[1]
    elif sp_reso:
        ret['sensor'] = reso[0] if len(reso) == 1 or sp_reso['W'] > reso[1]['W'] else reso[1]
    else:
        raise Exception('Mp and SP both are NULL')

    if mp_reso:
        ret['mp_crop'] = cal_crop(ret['sensor'], mp_reso)
    if sp_reso:
        ret['sp_crop'] = cal_crop(ret['sensor'], sp_reso)

    return ret

def gen_settings(root, key, sensor, sp_crop, sp, mp_crop, mp, mode):
    """ for one stream, the sp and sp_crop are None """

    active_outputs = 2 if sp else 1
    setting = ET.SubElement(root, 'settings',
                            attrib={'key': str(key),
                                    'id' : str(1),
                                    'fps': str(sensor['FPS']),
                                    'active_outputs': str(active_outputs)})
    video = ET.SubElement(setting, 'video',
                          attrib={'width': str(mp['W']),
                                  'height': str(mp['H']),
                                  'stream_id': str(1),
                                  'format': 'NV12',
                                  'peer': 'output'})
    v_crp = ET.SubElement(video, 'pcrp',
                          attrib={'width': str(mp_crop['W']),
                                  'height': str(mp_crop['H'])})
    v_rsz = ET.SubElement(video, 'prsz',
                          attrib={'width': str(mp['W']),
                                  'height': str(mp['H'])})
    if sp and sp_crop:
        preview = ET.SubElement(setting, 'preview',
                                attrib={'width': str(sp['W']),
                                        'height': str(sp['H']),
                                        'stream_id': str(1),
                                        'format': 'NV12',
                                        'peer': 'output'})
        p_crp = ET.SubElement(preview, 'pcrp',
                              attrib={'width': str(sp_crop['W']),
                                      'height': str(sp_crop['H'])})
        p_rsz = ET.SubElement(preview, 'prsz',
                              attrib={'width': str(sp['W']),
                                      'height': str(sp['H'])})
    else:
        preview = ET.SubElement(setting, 'preview', attrib={'enabled': str(0)})

    still = ET.SubElement(setting, 'still', attrib={'enabled': str(0)})

    et_sensor = ET.SubElement(setting, 'sensor',
                              attrib={'stream_id': str(1),
                                      'vflip': str(0),
                                      'hflip': str(0),
                                      'mode_id': str(sensor['W']) + 'x' + str(sensor['H']),
                                      'analogue_gain': str(100),
                                      'exposure': str(500)})
    et_sensor_port = ET.SubElement(et_sensor, 'port_0',
                                   attrib={'type': 'port',
                                           'width': str(sensor['W']),
                                           'height': str(sensor['H']),
                                           'format': mode['sensor_fmt']})
    csi = ET.SubElement(setting, 'csi_be_soc')
    csi_output = ET.SubElement(csi, 'output',
                               attrib={'width': str(sensor['W']),
                                       'height': str(sensor['H']),
                                       'format': mode['sensor_fmt']})

    imgu = ET.SubElement(setting, 'imgu', attrib={'stream_id': str(1)})
    imgu_input = ET.SubElement(imgu, 'input',
                               attrib={'width': str(sensor['W']),
                                       'height': str(sensor['H']),
                                       'format': mode['sensor_fmt']})
    iac = ET.SubElement(imgu_input, 'iac',
                        attrib={'width': str(sensor['W']),
                                'height': str(sensor['H'])})
    imgu_output = ET.SubElement(imgu, 'output',
                                attrib={'width': str(sensor['W']),
                                       'height': str(sensor['H']),
                                       'format': 'NV16',
                                       'pipe_output_id': str(0)})
    ism = ET.SubElement(imgu_output, 'ism',
                        attrib={'width': str(sensor['W']),
                                'height': str(sensor['H'])})

def gen_sensor_modes(root, reso, mode):
    et_mode = ET.SubElement(root, 'sensor_modes',
                            attrib={'sensor_name': mode['name'],
                                    'i2c_address': mode['i2c_address'],
                                    'csi_port': '0',
                                    'metadata': '0',
                                    'bayer_order': mode['bayer_order']})
    et_bpp = ET.SubElement(et_mode, 'available_bit_per_pixel')
    et_bpp_value = ET.SubElement(et_bpp, 'bpp', attrib={'value': str(mode['bpp'])})

    et_pll = ET.SubElement(et_mode, 'pll_configs')
    et_pll_item = ET.SubElement(et_pll, 'pll_config',
                                attrib={'id': '0',
                                        'pixel_rate': str(mode['pixel_rate']),
                                        'pixel_rate_csi': str(mode['pixel_rate']),
                                        'bpp': str(mode['bpp'])})
    i = 0
    item_max = reso[0]
    for item in reso:
        et_item = ET.SubElement(et_mode, 'sensor_mode',
                                attrib={'name': str(item['W']) + 'x' + str(item['H']),
                                        'id': str(i),
                                        'width': str(item['W']),
                                        'height': str(item['H']),
                                        'fps': str(item['FPS']),
                                        'csi_port': '0',
                                        'scaler_pad': '1',
                                        'target': '0',
                                        'min_fll': '0',
                                        'min_llp': '0'})
        i = i + 1

        et_pixel = ET.SubElement(et_item, 'pixel_array')
        et_pixel_input = ET.SubElement(et_pixel, 'input',
                                       attrib={'width': str(item_max['W']),
                                               'height': str(item_max['H']),
                                               'left': '0',
                                               'top': '0'})
        et_pixel_output = ET.SubElement(et_pixel, 'output',
                                        attrib={'width': str(item['W']),
                                                'height': str(item['H']),
                                                'left': '0',
                                                'top': '0'})
        et_binner = ET.SubElement(et_item, 'binner',
                                  attrib={'h_factor': str(item['binner_h']),
                                          'v_factor': str(item['binner_w'])})
        # binner input === max reso ?
        et_binner_input = ET.SubElement(et_binner, 'input',
                                        attrib={'width': str(item_max['W']),
                                                'height': str(item_max['H']),
                                                'left': '0',
                                                'top': '0'})
        et_binner_output = ET.SubElement(et_binner, 'output',
                                         attrib={'width': str(item['W']),
                                                 'height': str(item['H']),
                                                 'left': '0',
                                                 'top': '0'})

def generate_xml(fn, reso, mode, target):
    sp_max = {'W': 1920, 'H': 1080}

    root = ET.Element('graph_settings')

    gen_sensor_modes(root, reso, mode)

    key = 7000
    for mp in target:
        for sp in target:
            if sp['W'] > mp['W'] or sp['W'] > sp_max['W']:
                continue
            ret = cal_reso(reso, sp, mp)
            gen_settings(root, key, ret['sensor'], ret['sp_crop'], sp, ret['mp_crop'], mp, mode)
            key = key + 1

    key = 8000
    for mp in target:
        ret = cal_reso(reso, None, mp)
        gen_settings(root, key, ret['sensor'], None, None, ret['mp_crop'], mp, mode)
        key = key + 1

    # ET.dump(root)
    xml = minidom.parseString(ET.tostring(root)).toprettyxml(indent='  ')
    with open(fn, 'w') as f:
        f.write(xml)

def get_user_paras():
    try:
        opt = OptionParser()
        opt.add_option('--name',
                dest="name",
                type=str,
                default="none",
                help="sensor name which could be got from sys/class/video4linux/v4l-subdev2/name or cmd: media-ctl -p")
        opt.add_option('--i2c_address',
                dest="i2c_address",
                type=str,
                default="0-0000",
                help="sensor i2c_address which could be got from sys/class/video4linux/v4l-subdev2/name or cmd: media-ctl -p")
        opt.add_option('--bayer_order',
                dest="bayer_order",
                type=str,
                default="BGGR",
                help="sensor bayer_order which could be got from cmd: media-ctl -p")
        opt.add_option('--sensor_fmt',
                dest="sensor_fmt",
                type=str,
                default="BG10",
                help="sensor fmt which could be got from cmd: media-ctl -p")
        opt.add_option('--bpp',
                dest="bpp",
                type=int,
                default=10,
                help="sensor bpp")
        opt.add_option('--pixel_rate',
                dest="pixel_rate",
                type=int,
                default=180000000,
                help="sensor pixel_rate ")
        opt.add_option('--width',
                dest="width",
                type=int,
                default=0,
                help="sensor output width")
        opt.add_option('--height',
                dest="height",
                type=int,
                default=0,
                help="sensor output height")
        opt.add_option('--binner_width',
                dest="binner_width",
                type=int,
                default=0,
                help="sensor output binner width")
        opt.add_option('--binner_height',
                dest="binner_height",
                type=int,
                default=0,
                help="sensor output binner height")

        (options, args) = opt.parse_args()

        is_valid_paras = True
        error_messages = ["Config Err:"]
        if not options.name:
            error_messages.append("--name must be set;")
            is_valid_paras = False
        if not options.width:
            error_messages.append("--width must be set;")
            is_valid_paras = False
        if not options.height:
            error_messages.append("--height must be set;")
            error_messages.append("if you do not clear how to config all the options, just config name, width, height")
            is_valid_paras = False

        if is_valid_paras:
            return options
        else:
            for error_message in error_messages:
                print(error_message)
            opt.print_help()
            return None

    except Exception as ex:
        print("exception :{0}".format(str(ex)))
        return None

def main(argv):
    opts = get_user_paras()
    if not opts:
        return None
    else:
        reso = [{'W': opts.width, 'H': opts.height, 'FPS': 30, 'binner_h': 1, 'binner_w': 1}]
        if opts.binner_width and opts.binner_height:
            reso.append({'W': opts.binner_width, 'H': opts.binner_height, 'FPS': 60,
                'binner_h': 2, 'binner_w': 2})
        mode = {'name': opts.name,
                'i2c_address': opts.i2c_address,
                'bayer_order': opts.bayer_order,
                'bpp': opts.bpp,
                'pixel_rate': opts.pixel_rate,
                'sensor_fmt': opts.sensor_fmt}
        settings = [{'W': 1920, 'H': 1080},
                  {'W': 1280, 'H': 960},
                  {'W': 1280, 'H': 720},
                  {'W': 800, 'H': 600},
                  {'W': 640, 'H': 480},
                  {'W': 352, 'H': 288},
                  {'W': 320, 'H': 240},
                  {'W': 176, 'H': 144}]
        target = []
        for s in settings:
            if opts.width > s["W"] and opts.height > s["H"]:
                target.append(s)

        target.insert(0, {'W': opts.width, 'H': opts.height})

        if opts.binner_width and opts.binner_height:
            binners = {'W': opts.binner_width, 'H': opts.binner_height}
            if binners not in target:
                target.insert(0, binners)
        target.sort(reverse=True)
        print reso
        print mode
        print target
        generate_xml('./graph_settings_' + opts.name + '.xml', reso, mode, target)
        print "generate " + './graph_settings_' + opts.name + '.xml'

if __name__ == '__main__':
    main(sys.argv)
