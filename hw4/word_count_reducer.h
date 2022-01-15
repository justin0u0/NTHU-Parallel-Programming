#ifndef _WORD_COUNT_REDUCER_H_
#define _WORD_COUNT_REDUCER_H_

#include <fstream>
#include <iostream>
#include <vector>

#include "word_count_mapper.h"
#include "word_count_types.h"

class WordCountReducer {
private:
  // the ID of the reducer, range from 1 ~ config->numReducers
  int id;
  // the task ID of the reducer, range from 0 ~ config->numReducers-1,
  // for matching the partition key
  int taskId;

  WordCountConfig* config;

  void (*callback)(int, int);

  std::vector<ListWordCountKV*>* fetch() {
    std::vector<ListWordCountKV*>* output = new std::vector<ListWordCountKV*>;
    output->reserve(config->numMappers);

    for (int i = 1; i <= config->numMappers; ++i) {
      std::string filename = WordCountMapper::outputFilePath(i, config);
      std::ifstream f(filename);

      // the partition hash will be in the range of 0 ~ config->numReducers-1
      int partitionId;
      std::string key;
      int value;

      ListWordCountKV* data = new ListWordCountKV;

      while (f >> partitionId >> key >> value) {
        if (partitionId == taskId) {
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
    VectorWordCountKVs* output = new VectorWordCountKVs;

    for (const WordCountKV& kv : (*input)) {
      if (output->empty() || output->back().compare(kv) != 0) {
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
    std::string filename = WordCountReducer::outputFilePath(id, config);
    std::ofstream f(filename);

    for (const WordCountKV& kv : (*input)) {
      f << kv.key << ' ' << kv.value << '\n';
    }

    delete input;
  }

public:
  WordCountReducer(int id, int taskId, WordCountConfig* config, void (*callback)(int, int))
    : id(id), taskId(taskId), config(config), callback(callback) {}

  static std::string outputFilePath(int taskId, WordCountConfig* config) {
    return config->outputDir + config->jobName + "-" + std::to_string(taskId) + ".out";
  }

  static void* run(void* arg) {
    WordCountReducer* reducer = (WordCountReducer*)arg;

    std::vector<ListWordCountKV*>* fetchResult = reducer->fetch();
    // std::cout << "fetch done" << std::endl;

    VectorWordCountKV* mergeResult = reducer->merge(fetchResult);
    // std::cout << "merge done" << std::endl;

    VectorWordCountKVs* groupResult = reducer->group(mergeResult);
    // std::cout << "group done" << std::endl;

    VectorWordCountKV* reduceResult = reducer->reduce(groupResult);
    // std::cout << "reduce done" << std::endl;

    reducer->write(reduceResult);
    // std::cout << "write done" << std::endl;

    (*(reducer->callback))(reducer->id, reducer->taskId);

    delete reducer;

    return nullptr;
  }
};

#endif  // _WORD_COUNT_REDUCER_H_
