# Author: justin0u0<mail@justin0u0.com>
# Usage:
# - run specific test case: python3 runner.py 01
# - run all test cases: python3 runner.py all

import os
import sys
import json
import hashlib

def run_test(test_dir, test_name, testcase):
  with open(f'{test_dir}/{testcase}.txt', 'r') as f:
    spec = {
      'nnode': 1,
      'nproc': 1,
      'ncpus': 1
    }

    for line in f.readlines():
      k, v = line.split('=')
      spec[k] = int(v)

    print('\033[1;34;48m' + '='*35 + f' Start running test {testcase} ' + '='*35 + '\033[1;37;0m')
    print('spec:', spec)

    main_script = f'srun -N{spec["nnode"]} -n{spec["nproc"]} -c{spec["ncpus"]} ./lab2_{test_name} {spec["r"]} {spec["k"]} > out'
    print(main_script)
    os.system(main_script)
    print('done.')

    print('')
    print('verifying ...')

    output = 0
    with open('out', 'r') as ff:
      output = int(ff.readline())

    if output != spec["answer"]:
      print('\033[1;31;48m' + 'fail' + '\033[1;37;0m')
    else:
      print('\033[1;32;48m' + 'success' + '\033[1;37;0m')

test_dir = ''
test_name = ''
testcase = '01'
if len(sys.argv) > 2:
  test_name = sys.argv[1]
  testcase = sys.argv[2]
else:
  raise BaseException('insufficient arguments')

if test_name == 'pthread':
  test_dir = './testcases_1'
elif test_name == 'omp':
  test_dir = './testcases_2'
elif test_name == 'hybrid':
  test_dir = './testcases_3'
else:
  raise BaseException('invalid test name, only "pthread", "omp" or "hybrid" is allowed')

if testcase == 'all':
  for i in range(1, 13):
    run_test(test_dir, test_name, str(i).zfill(2))
else:
  run_test(test_dir, test_name, testcase)
