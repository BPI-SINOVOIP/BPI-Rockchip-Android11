## Combine images

### Usage
``python mk_combined_img.py [--input <filename> --output <filename>]``
The defaults of `--input` and `--output` are:
* `./image_config` for `--input`
* `$OUT/combined.img` for `--output`

### Prerequisite
The script will try to get environment variables ``$ANDROID_HOST_OUT`` and ``$OUT``
If you have built android these variables should exist and in place.
Make sure simg2img and sgdisk are in ``$ANDROID_HOST_OUT``, if not, do following:

``$ cd $ANDROID_BUILD_TOP``

``$ m simg2img``

``$ m sgdisk``

### Functionality
* Combine multiple images into one image with multiple partitions
* Sparse image detection

### The format of input config file
Each line with the order of ``</path/to/image>`` ``<partition label>`` ``<partition number>``


``<partition number>`` should be within range of ``[1, number of lines]``
and cannot be repeated.

### Config file example
```
$OUT/sparse_system.img      system      1
$OUT/encryptionkey.img      encrypt     4
$OUT/vendor.img             vendor      5
$OUT/sparse_userdata.img    userdata    3
$OUT/cache.img              cache       2
```

### TODO
* Output in sparse format
* Detect images that already have partitions in them.
* Auto genereate config file

