import argparse as ap
import random

parser = ap.ArgumentParser(description='Generate hw4 test and locality file.')
parser.add_argument('action', choices=['word', 'chunk'])
parser.add_argument('--dims', '-d', nargs=2)
parser.add_argument('--total-words')
parser.add_argument('--chunk-num')
parser.add_argument('--output', '-o', required=True)
# args = parser.parse_args('word --dims 5 10 --total-words 5 -o test.out'.split(' '))
# args = parser.parse_args('chunk --chunk-size 15 -o test_locality.out'.split(' '))
args = parser.parse_args()
if args.action == 'word':
    with open('/home/pp21/pp21t00/hw4/words.txt') as in_file:
        word_list = in_file.read().split('\n')[:-1]
        if args.total_words:
            word_list = random.sample(word_list, int(args.total_words))
        selected_list = []
        M, N = list(map(int, args.dims))
        for i in range(M):
            selected_list.append(random.choices(word_list, k=N))

        with open(args.output, 'w') as out_file:
            out_file.write('\n'.join([' '.join(line) for line in selected_list]))

elif args.action == 'chunk':
    chunk_list = []
    for i in range(int(args.chunk_num)):
        chunk_list.append(f'{i+1} {random.randint(1, 8)}')
    
    with open(args.output, 'w') as out_file:
        out_file.write('\n'.join(chunk_list))


        
        

