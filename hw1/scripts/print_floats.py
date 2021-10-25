# Author: justin0u0<mail@justin0u0.com>
# Usage: python3 print_floats.py ./input_file.txt

import sys
import struct

limit = 40

with open(sys.argv[1], 'rb') as f:
  byte = f.read(4)
  while byte != b'':
    print(byte, struct.unpack('f', byte)[0])	
    byte = f.read(4)

    limit -= 1
    if limit == 0:
      break
