import sys
import  os
import zipfile

def pack_loader():
    if (len(sys.argv) != 3) :
        print("Usage: package_loader_zip.py [RKLoader.bin] [update_unsign.zip]")
        return
    if ((sys.argv[1] == "-h") or (sys.argv[1] == "--h")):
        print("Usage: package_loader_zip.py [RKLoader.bin] [update_unsign.zip]")
        return
    print("arvg[1]=%s argv[2]=%s \n" % (sys.argv[1], sys.argv[2]))
    output_zip = zipfile.ZipFile(sys.argv[2], "w", compression=zipfile.ZIP_DEFLATED)
    output_zip.write(sys.argv[1],"RKLoader.bin")
    output_zip.close()
    print("Success!")


if __name__ == '__main__':
    pack_loader()
