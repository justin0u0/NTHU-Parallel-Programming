#include <vector>
#include "word_count_reducer.h"
using namespace std;

int main() {
	vector<string> inputFiles{"./tests/temp_00.txt", "./tests/temp_01.txt", "./tests/temp_02.txt"};

	string reducer0OutputFile = "./tests/out_00.txt";
	WordCountReducer* reducer0 = new WordCountReducer(1, 1, inputFiles, reducer0OutputFile);
	string reducer1OutputFile = "./tests/out_01.txt";
	WordCountReducer* reducer1 = new WordCountReducer(2, 2, inputFiles, reducer1OutputFile);

	WordCountReducer::run((void*)reducer0);
	WordCountReducer::run((void*)reducer1);
}
