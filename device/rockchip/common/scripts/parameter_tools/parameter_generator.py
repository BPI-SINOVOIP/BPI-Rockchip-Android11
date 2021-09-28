#!/usr/bin/env python
import sys
import getopt
import os
from string import Template

usage = 'Invalid Parameters! Example:\nparameter_tools --firmware-version 10.0 --machine-model RK3326 --manufacturer ROCKCHIP --machine XTF863 --partition-list uboot_a:4096K,trust_a:4M,misc:4M,dtbo_a:4M,vbmeta_a:4M,boot_a:33554432,backup:300M,security:4M,cache:300M,metadata:4096,frp:512K,super:2G --output te.txt'

# size: K/M/G
# return int
def calculate_blocks(size):
    level = size[-1]
    part_size = int(size[:-1])
    if level == 'K':
        part_size = part_size * 1024
    elif level == 'M':
        part_size = part_size * 1024 * 1024
    elif level == 'G':
        part_size = part_size * 1024 * 1024 * 1024
    else:
        part_size = int(size)
    # convert to blocks
    return part_size / 512

# return end blocks pos
def generate_pt(pt_name,pt_size,pt_start):
    part_start = "{:#010x}".format(pt_start)
    if pt_name == 'userdata':
        part_size = '-'
        return "-@" + part_start + "(userdata:grow)"

    part_size = "{:#010x}".format(calculate_blocks(pt_size))
    result = part_size + "@" + part_start + "(" + pt_name + ")"
    return result, int(calculate_blocks(pt_size) + pt_start)

def main(argv):
    ifile = 'parameter.in'
    type = 'GPT'
    start_offset = 16384
    firmware_version = ''
    machine_model = ''
    machine_id = '007'
    manufacturer = ''
    magic = '0x5041524B'
    atag = '0x00200800'
    machine = ''
    check_mask = '0x80'
    pwr_hld = '0,0,A,0,1'
    ofile = ''
    partition_list = ''
    try:
        opts, args = getopt.getopt(argv, "h", ["input=","start-offset=","firmware-version=","machine-model=","machine-id=","manufacturer=","magic=","atag=","machine=","check-mask=","pwr-hld=","partition-list=","output="])
    except getopt.GetoptError:
        print (usage)
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print (usage)
            sys.exit(2)
        elif opt == "--input":
            ifile = arg;
        elif opt == "--start-offset":
            start_offset = arg;
        elif opt == "--firmware-version":
            firmware_version = arg;
        elif opt == "--machine":
            machine = arg
        elif opt == "--machine-model":
            machine_model = arg;
        elif opt == "--machine-id":
            machine_id = arg;
        elif opt == "--manufacturer":
            manufacturer = arg;
        elif opt == "--magic":
            magic = arg;
        elif opt == "--atag":
            atag = arg;
        elif opt == "--check-mask":
            check_mask = arg;
        elif opt == "--pwr-hld":
            pwr_hld = arg;
        elif opt == "--partition-list":
            partition_list = arg;
        elif opt == "--output":
            ofile = arg;
        else:
            print (usage)
            sys.exit(2)

    if partition_list == '':
        print (usage)
        sys.exit(2)
    # append '_b' parts if '_a' is exists.
    list_partitions = partition_list.split(',')
    cur_part_start = int(start_offset)
    final_parts = ''
    for cur_part in list_partitions:
        pos_split = cur_part.find(':')
        cur_part_name = cur_part[:pos_split]
        pos_slotselect = cur_part.find('_a')

        cur_part_size = cur_part[1 + pos_split:]
        str_part, int_next_start_offset = generate_pt(cur_part_name, cur_part_size, cur_part_start)
        cur_part_start = int_next_start_offset
        final_parts += str_part + ","
        if pos_slotselect >= 0:
            slot_b_part_name = cur_part[:pos_slotselect] + '_b'
            str_part, int_next_start_offset = generate_pt(slot_b_part_name, cur_part_size, cur_part_start)
            cur_part_start = int_next_start_offset
            final_parts += str_part + ","

    partition_list = final_parts + generate_pt('userdata', '0', cur_part_start)

    file_parameter_in = open(ifile)
    template_parameter_in = file_parameter_in.read()
    template_in_t = Template(template_parameter_in)

    line = template_in_t.substitute(_firmware_version=firmware_version,_machine_model=machine_model,_machine_id=machine_id,_manufacturer=manufacturer,_magic=magic,_atag=atag,_machine=machine,_check_mask=check_mask,_pwr_hld=pwr_hld,_type=type,_partition_list=partition_list)

    if ofile != '':
        with open(ofile,"w") as f:
            f.write(line)
    else:
        print (line)

if __name__=="__main__":
    main(sys.argv[1:])
