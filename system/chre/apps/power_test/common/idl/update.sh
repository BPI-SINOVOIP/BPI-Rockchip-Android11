#!/bin/bash

# NOTE: Ensure you use flatc version 1.6.0 here!

# Generate the CHRE-side header file
flatc --cpp -o ../include/generated/ --scoped-enums \
  --cpp-ptr-type chre::UniquePtr chre_power_test.fbs

# Generate the AP-side header file with some extra goodies
flatc --cpp -o ./ --scoped-enums --gen-mutable \
  --gen-object-api chre_power_test.fbs
