import os
import sys

def run_test(test_name, test_case):
	with open(f'./testcases/{test_case}.txt', 'r') as f:
		spec = {}

def main():
	assert(len(sys.argv) == 3)
	
	test_name = sys.argv[1]
	assert(test_name is 'hw2a' or test_name is 'hw2b')

	testcase = sys.argv[2]

	if testcase == 'all':
		for file_name in os.listdir('./testcases'):
			if file_name.endswith('.txt'):
				run_test(test_name, file_name.split('.')[0])


if __name__ == '__main__':
	main()
