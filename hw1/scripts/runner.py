# Author: justin0u0<mail@justin0u0.com>
# Usage:
# - run specific test case: python3 runner.py 01
# - run all test cases: python3 runner.py all
# Note: Testcase 26, 36 will fail, since they are all zeros and order does not matters.

import os
import sys
import json
import hashlib
import subprocess

def calc_hash(file_name):
  sha256_hash = hashlib.sha256()
  with open(file_name, 'rb') as f:
    for byte_block in iter(lambda: f.read(4096), b''):
      sha256_hash.update(byte_block)

  h = sha256_hash.hexdigest()
  print(file_name.ljust(20), h)
  return h

def run_test(testcase):
  with open(f'./testcases/{testcase}.txt', 'r') as f:
    spec = json.load(f)

    print('\033[1;34;48m' + '='*35 + f' Start running test {testcase} ' + '='*35 + '\033[1;37;0m')
    print('spec:', spec)

    out_file_name = f'./out'
    os.system(f'rm {out_file_name}')

    main_script = f'srun -N{spec["nodes"]} -n{spec["procs"]} ./hw1 {spec["n"]} ./testcases/{testcase}.in {out_file_name}'
    print(main_script)
    os.system(main_script)
    print('done.')

    print('')
    print('verifying ...')
    if calc_hash(out_file_name) != calc_hash(f'./testcases/{testcase}.out'):
      print('\033[1;31;48m' + 'fail' + '\033[1;37;0m')
    else:
      print('\033[1;32;48m' + 'success' + '\033[1;37;0m')

testcase = '01'
if len(sys.argv) > 1:
  testcase = str(sys.argv[1])

if testcase == 'all':
  for i in range(1, 41):
    run_test(str(i).zfill(2))
else:
  run_test(testcase)
