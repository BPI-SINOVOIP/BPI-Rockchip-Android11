#! /usr/bin/env python
# -*- coding: utf-8 -*-
"""
Created on Fri Jun  9 15:54:32 2017

@author: LY
"""
import os, struct, sys

def usage(argv0):
  print("""
Usage: %s sparse_image_file [align_unit_K]
""" % ( argv0 ))
  sys.exit(-1)

def main():
    me = os.path.basename(sys.argv[0])
    if len(sys.argv) < 2:
        print("Error: Not enough param")
        usage(me)
    in_file_path = sys.argv[1]
    out_file_path = in_file_path + ".out"
    if len(sys.argv) > 2:
        align_unit = int(sys.argv[2])
    else:
        align_unit = 1024
    print("in=%s out=%s align=%u" % (in_file_path, out_file_path, align_unit))
    try:
        fin = open(in_file_path, 'rb')
        fout = open(out_file_path, 'wb')
        head_bin = fin.read(28)
        head_struct = list(struct.unpack("<I4H4I", head_bin))

        magic = head_struct[0]
        major_version = head_struct[1]
        minor_version = head_struct[2]
        file_hdr_sz = head_struct[3]
        chunk_hdr_sz = head_struct[4]
        blk_sz = head_struct[5]
        total_blks = head_struct[6]
        total_chunks = head_struct[7]
        image_checksum = head_struct[8]
        
        if magic == 0xED26FF3A:
            print("Total of %u %u-byte output blocks in %u input chunks."
                  % (total_blks, blk_sz, total_chunks))
            out_total_chunks = 0
            out_head = bytearray(28)
            out_chunk = bytearray(12)
            chunk_struct = list(struct.unpack("<2H2I", out_chunk))
            buffer = bytearray(align_unit * 1024)
            offset = 0
            i = 0
            remain_count = align_unit * 1024
            fout.write(out_head)
            while i < total_chunks:
                in_chunk = fin.read(12)
                in_chunk_struct = struct.unpack("<2H2I", in_chunk)
                chunk_type = in_chunk_struct[0]
                reserved1 = in_chunk_struct[1]
                chunk_sz = in_chunk_struct[2]
                total_sz = in_chunk_struct[3]
                data_sz = total_sz - 12
                fill_value = 0
                fill_bin = bytearray(4)
                if chunk_type == 0xCAC1:#raw chunk
                    data_sz = chunk_sz * blk_sz
                elif chunk_type == 0xCAC2:#fill chunk
                    fill_bin = fin.read(4)
                    fill_value = struct.unpack("<I", fill_bin)
                    data_sz = chunk_sz * blk_sz
                elif chunk_type == 0xCAC3:#don't care chunk
                    data_sz = chunk_sz * blk_sz
                else:
                    print("Error: Unknown chunk %04X" % (chunk_type))
                    break
                if offset == 0:
                    chunk_struct[0] = chunk_type
                    chunk_struct[1] = 0
                    chunk_struct[2] = 0
                    chunk_struct[3] = 12
                    remain_count = align_unit * 1024
                if data_sz > remain_count:
                    if offset == 0:
                        fout.write(in_chunk)
                        if chunk_type == 0xCAC1:
                            fout.write(fin.read(data_sz))
                        elif chunk_type == 0xCAC2:
                            fout.write(fill_bin)
                    else:
                        if chunk_struct[0] != 0xCAC3:
                            fout.write(struct.pack("<2H2I",*chunk_struct))
                            fout.write(buffer[0:offset])
                        else:
                            chunk_struct[3] = 12
                            fout.write(struct.pack("<2H2I",*chunk_struct))
                        if chunk_type == 0xCAC2:
                            fin.seek(-16,os.SEEK_CUR)
                        else:
                            fin.seek(-12,os.SEEK_CUR)
                        i -= 1
                    out_total_chunks += 1
                    offset = 0
                elif data_sz == remain_count:
                    if offset == 0:
                        fout.write(in_chunk)
                        if chunk_type == 0xCAC1:
                            fout.write(fin.read(data_sz))
                        elif chunk_type == 0xCAC2:
                            fout.write(fill_bin)
                    else:
                        if chunk_type == 0xCAC1:#raw chunk
                            if chunk_struct[0] != 0xCAC1:
                                chunk_struct[0] = 0xCAC1
                            chunk_struct[2] += chunk_sz
                            chunk_struct[3] += data_sz
                            buffer[offset:] = fin.read(data_sz)
                        elif chunk_type == 0xCAC2:#fill chunk
                            if chunk_struct[0] != 0xCAC1:
                                chunk_struct[0] = 0xCAC1
                            chunk_struct[2] += chunk_sz
                            chunk_struct[3] += data_sz
                            buffer[offset:] = [(fill_value % 256) for j in range(data_sz)]
                        elif chunk_type == 0xCAC3:#don't care chunk
                            buffer[offset:] = [0 for j in range(data_sz)]
                            chunk_struct[2] += chunk_sz
                            chunk_struct[3] += data_sz
                        offset += data_sz
                        remain_count -= data_sz
                        if chunk_struct[0] == 0xCAC3:
                            chunk_struct[3] = 12
                        fout.write(struct.pack("<2H2I",*chunk_struct))
                        if chunk_struct[0] != 0xCAC3:
                            fout.write(buffer)
                    out_total_chunks += 1
                    offset = 0
                else:
                    if chunk_type == 0xCAC1:#raw chunk
                        if chunk_struct[0] != 0xCAC1:
                            chunk_struct[0] = 0xCAC1
                        chunk_struct[2] += chunk_sz
                        chunk_struct[3] += data_sz
                        buffer[offset:offset + data_sz] = fin.read(data_sz)
                    elif chunk_type == 0xCAC2:#fill chunk
                        if chunk_struct[0] != 0xCAC1:
                            chunk_struct[0] = 0xCAC1
                        chunk_struct[2] += chunk_sz
                        chunk_struct[3] += data_sz
                        buffer[offset:offset + data_sz] = [(fill_value % 256) for j in range(data_sz)]
                    elif chunk_type == 0xCAC3:#don't care chunk
                        buffer[offset:offset + data_sz] = [0 for j in range(data_sz)]
                        chunk_struct[2] += chunk_sz
                        chunk_struct[3] += data_sz
                    offset += data_sz
                    remain_count -= data_sz
                i += 1
            if offset > 0:
                if chunk_struct[0] == 0xCAC3:
                    chunk_struct[3] = 12
                fout.write(struct.pack("<2H2I",*chunk_struct))
                if chunk_struct[0] != 0xCAC3:
                    fout.write(buffer)
                out_total_chunks += 1
                offset = 0;    
            fout.seek(0, os.SEEK_SET)
            head_struct[7] = out_total_chunks
            fout.write(struct.pack("<I4H4I", *head_struct))
            fin.close()
            fout.close()
            print("Generating optimized sparse image done,total_chunk=%u." % (out_total_chunks))
        else:
            print("Error: %s: Magic should be 0xED26FF3A but is 0x%08X" % (in_file_path, magic))
    except Exception as e:
        print("Exception:" + str(e))
        print("i=%u" % (i))
        if fin:
            fin.close()
        if fout:
            fout.close()
    sys.exit(0)
        
if __name__ == "__main__":
  main()
