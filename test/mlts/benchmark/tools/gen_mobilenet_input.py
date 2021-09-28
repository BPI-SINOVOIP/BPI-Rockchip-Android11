 #!/usr/bin/python3
""" Generate inputs for NNAPI benchamrks using a image

Usage:
./gen_mobilenet_input.py image_file.jpg
"""


from PIL import Image
import numpy as np
import sys
import os

def gen_input_files(filename, prefix, size):
  basename = os.path.basename(filename) + "_" + prefix
  img_f64 = np.array(Image.open(filename).resize(size)).astype(np.float) / 128 - 1
  with open(basename + "_f32.bin", "wb") as f:
    f.write(img_f64.astype('float32').tobytes())
    print("Saving: " + basename + "_f32.bin")
  with open(basename + "_u8.bin", "wb") as f:
    f.write(((img_f64 + 1) * 255).astype('uint8').tobytes())
    print("Saving: " + basename + "_u8.bin")

if __name__ == '__main__':
  if len(sys.argv) < 1:
    print("Usage:\n ./gen_mobilenet_input.py image_file.jpg\n")
    sys.exit(1)

  gen_input_files(sys.argv[1], "mobilenet", (224, 224))
  gen_input_files(sys.argv[1], "inceptionv3", (299, 299))
  gen_input_files(sys.argv[1], "hdrnet", (256, 256))
