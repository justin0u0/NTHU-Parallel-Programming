#ifndef _WORD_COUNT_REDUCER_H_
#define _WORD_COUNT_REDUCER_H_

#include <fstream>
#include <iostream>
#include <vector>
#include "word_count_types.h"

class WordCountReducer {
private:
	int id;
	int taskId;
	std::vector<std::string> inputFiles;
	std::string outputFile;

	std::vector<ListWordCountKV*>* fetch() {
		std::vector<ListWordCountKV*>* output = new std::vector<ListWordCountKV*>;
		output->reserve(inputFiles.size());

		for (const std::string& path : inputFiles) {
			std::ifstream f(path);

			int reducerId;
			std::string key;
			int value;

			ListWordCountKV* data = new ListWordCountKV;

			while (f >> reducerId >> key >> value) {
				if (reducerId == id) {
					data->emplace_back(key, value);
				}
			}

			output->emplace_back(data);
		}

		return output;
	}

	VectorWordCountKV* merge(std::vector<ListWordCountKV*>* input) {
		int len = (int)input->size();
		
		for (int i = 1; i < len; i <<= 1) {
			for (int j = 0; j + i < len; j += (i << 1)) {
				// inplace merge list[j] and list[j + i] into list[j]

				ListWordCountKV* lhs = input->at(j);
				ListWordCountKV* rhs = input->at(j + i);

				ListWordCountKV::iterator rit = rhs->begin();

				for (ListWordCountKV::iterator lit = lhs->begin(); lit != lhs->end(); ++lit) {
					while (rit != rhs->end() && (*rit) < (*lit)) {
						lhs->emplace(lit, *rit);
						++rit;
					}
				}

				while (rit != rhs->end()) {
					lhs->emplace(lhs->end(), *rit);
					++rit;
				}

				delete rhs;
			}
		}

		ListWordCountKV* l = input->at(0);
		VectorWordCountKV* output = new VectorWordCountKV(l->begin(), l->end());

		delete l;
		delete input;

		return output;
	}

	VectorWordCountKVs* group(VectorWordCountKV* input) {
		VectorWordCountKVs* output;

		for (const WordCountKV& kv : (*input)) {
			if (output->empty() || output->back().compare(kv) != 0){
				output->emplace_back(kv.key);
			}
			output->back().values.emplace_back(kv.value);
		}

		delete input;

		return output;
	}

	VectorWordCountKV* reduce(VectorWordCountKVs* input) {
		VectorWordCountKV* output = new VectorWordCountKV;
		output->reserve(input->size());

		for (const WordCountKVs& kvs : (*input)) {
			// reduce by sum aggregation
			int sum = 0;
			for (int value : kvs.values) {
				sum += value;
			}
			output->emplace_back(kvs.key, sum);
		}

		delete input;

		return output;
	}

	void write(VectorWordCountKV* input) {
		std::ofstream f(outputFile);

		for (const WordCountKV& kv : (*input)) {
			f << kv.key << ' ' << kv.value << '\n';
		}

		delete input;
	}

public:
	WordCountReducer(int id, int taskId, std::vector<std::string>& inputFiles, std::string& outputFile)
		: id(id), taskId(taskId), inputFiles(inputFiles), outputFile(outputFile) {}

	static void* run(void* argv) {
		WordCountReducer* reducer = (WordCountReducer*)argv;

		std::vector<ListWordCountKV*>* fetchResult = reducer->fetch();

		VectorWordCountKV* mergeResult = reducer->merge(fetchResult);

		VectorWordCountKVs* groupResult = reducer->group(mergeResult);

		VectorWordCountKV* reduceResult = reducer->reduce(groupResult);

		reducer->write(reduceResult);

		return nullptr;
	}
};

#endif // _WORD_COUNT_REDUCER_H_
